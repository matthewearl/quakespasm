import math
import struct
import sys

from pyquake import proto


def _patch_vec(old_vec, update):
    return tuple(v if u is None else u for v, u in zip(old_vec, update))


def generate_ghost_file(demo_file):
    serverinfo_rcvd = False
    view_entity = None
    time = None
    origin = None
    angle = None
    frame = None

    for msg_end, view_angle, msg in proto.read_demo_file(demo_file):
        # Sometimes demos have the end of a previous run with no server info, so
        # wait for the restart.  For demos with multiple maps just take the
        # first map.
        if msg.msg_type == proto.ServerMessageType.SERVERINFO:
            if serverinfo_rcvd:
                break
            serverinfo_rcvd = True
        if not serverinfo_rcvd:
            continue

        if msg.msg_type == proto.ServerMessageType.SETVIEW:
            view_entity = msg.viewentity

        if msg.msg_type == proto.ServerMessageType.TIME:
            time = msg.time

        if (msg.msg_type == proto.ServerMessageType.SPAWNBASELINE
                and msg.entity_num == view_entity):
            origin = msg.origin
            angle = msg.angles
            frame = msg.frame

        if (msg.msg_type == proto.ServerMessageType.UPDATE
                and msg.entity_num == view_entity):
            origin = _patch_vec(origin, msg.origin)
            angle = _patch_vec(angle, msg.angle)
            if msg.frame is not None:
                frame = msg.frame
            sys.stdout.buffer.write(
                struct.pack("<fffffffL", time,
                            *origin,
                            *(180 * x / math.pi for x in angle),
                            frame)
            )

        if msg.msg_type == proto.ServerMessageType.INTERMISSION:
            break


if __name__ == "__main__":
    with open(sys.argv[1], 'rb') as f:
        generate_ghost_file(f)
