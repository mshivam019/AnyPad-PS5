import socket
import unittest
import sender

class SenderTests(unittest.TestCase):
    def test_controls(self):
        self.assertEqual(sender.controller_state({}, {}), (0,128,128,128,128,0,0))
        state = sender.controller_state({0:-32767, 1:32767, 2:32767, 16:-32767}, {0x130:1})
        self.assertEqual(state, (0x4180,0,255,128,128,255,0))
        self.assertEqual(sender.controller_state({}, {0x13A:1,0x13B:1})[0], 0x10000)
        self.assertEqual(sender.controller_state({}, {0x13A:1,0x13D:1})[0], 0x100000)

    def test_real_udp_roundtrip(self):
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as rx, socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as tx:
            rx.bind(("127.0.0.1",0)); rx.settimeout(1)
            frame=sender.packet(bytes(range(16)),0xffffffff,sender.controller_state({},{}))
            tx.sendto(frame,rx.getsockname()); received,_=rx.recvfrom(100)
            self.assertEqual(len(received),36)
            self.assertEqual(received[:20],b"RGP1"+bytes(range(16)))
            self.assertEqual(received[20:28],bytes.fromhex("ffffffff00000000"))
            self.assertEqual(received[28:],bytes([128,128,128,128,0,0,0,0]))

if __name__ == "__main__": unittest.main()
