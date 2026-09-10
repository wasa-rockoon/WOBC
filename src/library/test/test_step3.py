import sys
import types
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "src" / "library"))

# packet.py only needs crc for Packet.checksum(), which these encode/decode
# tests do not call. Keep the test runnable in PlatformIO's minimal Python.
crc_stub = types.ModuleType("crc")
crc_stub.Calculator = object
crc_stub.Crc8 = types.SimpleNamespace(CCITT=0)
sys.modules.setdefault("crc", crc_stub)

from wcpp import Entry, Packet


TYPE_MASK = 0x80
FRAME_MASK = 0x1F
SEPARATION_UNIT = 0x41
COMPONENT = 0x00
COMMAND_ID = ord("n")


def make_can_id(packet, frame=0):
    type_and_id = packet.encode()[1]
    return (type_and_id << 21) | (packet.component_id << 13) | (
        packet.origin_unit_id << 5
    ) | frame


def can_round_trip(packet):
    encoded = packet.encode()
    base_id = make_can_id(packet)
    decoded = bytearray([encoded[0], (base_id >> 21) & 0xFF,
                         (base_id >> 13) & 0xFF, (base_id >> 5) & 0xFF])
    offset = 4
    frame = 0
    while offset < len(encoded):
        capacity = 7 if frame == 0 else 8
        decoded.extend(encoded[offset:offset + capacity])
        offset += capacity
        assert ((base_id + frame) & FRAME_MASK) == frame
        frame += 1
    return Packet.decode(bytes(decoded))


class HandlerModel:
    EXECUTED = 1
    REJECTED = 2
    INVALID = 4

    def __init__(self):
        self.cache = {}

    def handle(self, packet, payload_valid=True, requested_on=False):
        if (not packet.is_command() or not packet.is_remote()
                or packet.component_id != COMPONENT
                or packet.dest_unit_id != SEPARATION_UNIT
                or packet.origin_unit_id == 0):
            return self.REJECTED, False, False
        key = (packet.origin_unit_id, packet.packet_id, packet.sequence)
        if key in self.cache:
            status, _ = self.cache[key]
            return status, False, True
        if packet.packet_id != COMMAND_ID:
            result = (self.REJECTED, False)
        elif not payload_valid:
            result = (self.INVALID, False)
        else:
            result = (self.EXECUTED, True)
        self.cache[key] = result
        return result[0], result[1], False


class Step3Tests(unittest.TestCase):
    def packet(self, command=True, remote=True, sequence=1, destination=0x41,
               packet_id=COMMAND_ID):
        factory = Packet.command if command else Packet.telemetry
        packet = factory(packet_id, COMPONENT, 0x64 if remote else 0,
                         destination, sequence)
        packet.entries.append(Entry("On").set_int(1))
        return packet

    def test_can_round_trip_matrix(self):
        for command in (True, False):
            for remote in (True, False):
                for multi_frame in (False, True):
                    packet = self.packet(command, remote)
                    if multi_frame:
                        packet.entries.append(Entry("Tx").set_string("multi-frame"))
                    decoded = can_round_trip(packet)
                    self.assertEqual(decoded.encode(), packet.encode())
                    self.assertEqual(decoded.is_command(), command)
                    self.assertEqual(decoded.packet_id, packet.packet_id)
                    self.assertEqual(decoded.origin_unit_id, packet.origin_unit_id)
                    self.assertEqual(decoded.dest_unit_id,
                                     packet.dest_unit_id if remote else 0)
                    self.assertEqual(decoded.sequence,
                                     packet.sequence if remote else 0)

    def test_same_id_command_and_telemetry_have_distinct_can_ids(self):
        command = self.packet(True, False)
        telemetry = self.packet(False, False)
        self.assertEqual(command.packet_id, telemetry.packet_id)
        self.assertNotEqual(make_can_id(command), make_can_id(telemetry))

    def test_command_validation_and_duplicates(self):
        handler = HandlerModel()
        self.assertEqual(handler.handle(self.packet()), (1, True, False))
        self.assertEqual(handler.handle(self.packet()), (1, False, True))
        self.assertEqual(handler.handle(self.packet(sequence=2)), (1, True, False))
        self.assertEqual(handler.handle(self.packet(destination=0x42)), (2, False, False))
        self.assertEqual(handler.handle(self.packet(command=False)), (2, False, False))
        self.assertEqual(handler.handle(self.packet(packet_id=ord("x"))), (2, False, False))
        self.assertEqual(handler.handle(self.packet(sequence=3), False), (4, False, False))

    def test_application_ack_fields(self):
        command = self.packet(sequence=0x4567)
        ack = Packet.telemetry(ord("a"), COMPONENT, SEPARATION_UNIT,
                               command.origin_unit_id, command.sequence)
        ack.entries.extend([
            Entry("Ri").set_int(command.packet_id),
            Entry("Sq").set_int(command.sequence),
            Entry("St").set_int(1),
            Entry("Dp").set_int(0),
        ])
        decoded = Packet.decode(ack.encode())
        self.assertTrue(decoded.is_telemetry())
        self.assertEqual(decoded.origin_unit_id, SEPARATION_UNIT)
        self.assertEqual(decoded.dest_unit_id, command.origin_unit_id)
        self.assertEqual(decoded.sequence, command.sequence)
        self.assertEqual(decoded.find("Ri").int(), COMMAND_ID)
        self.assertEqual(decoded.find("Sq").int(), 0x4567)
        self.assertEqual(decoded.find("St").int(), 1)


if __name__ == "__main__":
    unittest.main()
