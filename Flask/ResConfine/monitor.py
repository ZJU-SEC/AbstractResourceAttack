from bcc import BPF, ct
from multiprocessing import Queue


BUFFER_SIZE = 16
RINGBUF_SIZE = 16
CORE_NUM = 16
USED_EBPF = 1

EBPF_PROG = [
    "monitors/monitor_inc_pcarray.c",
    "monitors/monitor_inc_mixhash.c",
]

process_status: dict[int, int] = dict()
container_status: dict[int, int] = dict()

pid_container: dict[int, int] = dict()
container_pids: dict[int, set[int]] = dict()


# C structs
class Profile(ct.Structure):
    _fields_ = [
        ("pid", ct.c_uint32),
        ("count", ct.c_int32),
        ("cgid", ct.c_uint32),
    ]


class MoniterConfig:
    def __init__(
        self,
        cgids: set[int],
        ceil: int,
        ra_id: int,
        ra_name: str,
        in_point: str,
        out_point: str,
        count_type: str,
        in_count: str,
        out_count: str,
    ):
        self.cgids = cgids
        self.ceil = ceil
        self.ra_id = ra_id
        self.ra_name = ra_name
        self.in_point = in_point
        self.out_point = out_point
        self.count_type = count_type
        self.in_count = in_count
        self.out_count = out_count


class MonitorOP:
    def __init__(self, cgid: int, status: int, msg=None):
        self.cgid = cgid
        self.status = status
        self.msg = msg


def monitor(init_event, queue: Queue, config: MoniterConfig):
    print("Create monitor for resource %s, start initing..." % config.ra_name)
    # print("[debug %d] ra_name: %s, in_point: %s, out_point: %s" % (ra_id, ra_name, in_point, out_point))

    # customize c codes
    prog = customize_prog(config.ra_name, config.in_count, config.out_count)
    b = BPF(text=prog, cflags=["-Wno-macro-redefined"])
    # print("[debug %d] load bpf done" % ra_id)

    # Init monitered containers
    for cgid in config.cgids:
        container_status[cgid] = 0
        container_pids[cgid] = set()
        key = ct.c_uint32(cgid)
        value = ct.c_uint32(1)
        b["ra_cgid_ra"][key] = value

    # closure with queue and config
    def create_handler(queue: Queue, config: MoniterConfig, is_test: bool):
        def handler(ctx, data, size):
            event = ct.cast(data, ct.POINTER(Profile)).contents
            global container_status
            global process_status
            global pid_container
            global container_pids

            # for i in range(event.size):
            pid = event.pid
            count = event.count
            cgid = event.cgid

            # Not what we need
            if cgid not in container_status:
                return

            process_status.setdefault(pid, 0)
            if config.count_type == "increment":
                process_status[pid] += count
                container_status[cgid] += count
            else:
                process_status[pid] = count
                container_status[cgid] = count
            
            # print("container_status[%d] = %d" % (cgid, container_status[cgid]))

        def test(ctx, data, size):
            event = ct.cast(data, ct.POINTER(Profile)).contents

            global container_status
            global process_status
            global pid_container
            global container_pids

            # for i in range(event.size):
            pid = event.pid
            count = event.count
            cgid = event.cgid

            # For testing
            process_status.setdefault(pid, 0)
            if config.count_type == "increment":
                process_status[pid] += count
                container_status[0] += count
            else:
                process_status[pid] = count
                container_status[0] = count

            queue.put(MonitorOP(0, container_status[0]))

        if is_test:
            return test
        else:
            return handler

    # print("[debug %d] fetch handler done" % ra_id)
    specific_handler = create_handler(queue, config, 0 in config.cgids)


    b.attach_kprobe(event=config.in_point.encode(), fn_name=b"allocate_monitor")
    # start poll
    # b["%s_events_%s" % (config.ra_name, config.ra_name)].open_ring_buffer(specific_handler)
    b["ra_events_ra"].open_ring_buffer(specific_handler)

    # print("[debug %d] all set done" % ra_id)
    print("Start moniter %s on containers " % config.ra_name)

    # set init done
    init_event.set()
    while 1:
        b.ring_buffer_poll()


def customize_prog(ra_name: str, in_count: str, out_count: str) -> str:
    # Read general monitor codes
    with open(EBPF_PROG[USED_EBPF], "r") as file:
        prog = file.read()

    prog = (
        prog.replace("__s32 count = inc_count;", "__s32 count = %s;" % in_count)
        .replace("__s32 count = dec_count;", "__s32 count = %s;" % out_count)
        .replace("CurrentAR", "%s" % ra_name)
        .replace("RA_BUFFER_SIZE_RA", str(BUFFER_SIZE))
        .replace("RA_RINGBUF_SIZE_RA", str(RINGBUF_SIZE))
        .replace("RA_CORE_NUM_RA", str(CORE_NUM))
    )
    # print(prog)

    return prog


def do_defence(container_id, ceiling, operation_type, resource_type="pid"):
    print(
        "Container : %s has reached the %s ceiling %d, so %s it"
        % (container_id, resource_type, ceiling, operation_type)
    )
    command = "docker " + operation_type + " " + container_id
    # output = os.popen(command)
    # print(output)
