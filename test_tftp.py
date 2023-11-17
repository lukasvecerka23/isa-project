import socket
import time
import struct

def send_ack(sock, block_number, server_address):
    ack_packet = struct.pack('!HH', 4, block_number)
    sock.sendto(ack_packet, server_address)

def exit_test(sock, server_address):
    exit_packet = b'\00\05\00\00\00'
    sock.sendto(exit_packet, server_address)


def test_wrong_request():
    server_address = ('127.0.0.1', 69)
    wrong_requests = [
        (b'\x00', 4),  # Too short
        (b'\x00\x01\x00\x00', 4),  # No filename
        (b'\x00\x01\x00\x00\x00\x00', 4),  # No null byte
        (b'\x00\x01test', 4),  # No null byte
        (b'\x00\x01test\x00', 4),  # No mode
        (b'\x00\x01test\x00octet', 4),  # No null byte
        (b'\x00\x01test\x00octettt\x00', 4), # wrong mode name
        (b'\x00\x03test\x00octet\x00', 4), # Wrong opcode
        (b'\x00\x01test\x00octet\x00\x00', 8), # Too long
        (b'\x00\x04test\x00octet\x00', 4), # Wrong opcode
        (b'\x00\x05test\x00octet\x00', 4), # Wrong opcode
        (b'\x00\x06test\x00octet\x00', 4), # Wrong opcode
        (b'\x00\x01test\x00octet\x00blksize\x00', 8),  # Missing value for blksize option
        (b'\x00\x01test\x00octet\x00blksize\x001024', 8),  # Missing null byte after blksize value
        (b'\x00\x01test\x00octet\x00blksize\x001024\x00timeout\x00', 8),  # Missing value for timeout option
        (b'\x00\x01test\x00octet\x00blksize\x001024\x00timeout\x0030', 8),  # Missing null byte after timeout value
        (b'\x00\x01test\x00octet\x00blksize\x001024\x00timeout\x0030\x00tsize\x00', 8),  # Missing value for tsize option
        (b'\x00\x01test\x00octet\x00blksize\x001024\x00timeout\x0030\x00tsize\x0010000', 8),  # Missing null byte after tsize value
        (b'\x00\x01test\x00octet\x00blksize\x001024\x00timeout\x0030\x00tsize\x0010000\x00option\x00', 8),  # Unknown option
        (b'\x00\x01test\x00octet\x00blksize\x001024\x00timeout\x0030\x00tsize\x0010000\x00option\x00value', 8),  # Missing null byte after unknown option value
    ]

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        for request, expected_error_code in wrong_requests:
            sock.sendto(request, server_address)
            data, _ = sock.recvfrom(1024)
            opcode, err_code = struct.unpack('!HH', data[:4])
            assert opcode == 5
            assert err_code == expected_error_code
            time.sleep(1)

def test_correct_rrq_wrq():
    server_address = ('127.0.0.1', 69)

    # Test cases for RRQ and WRQ with different modes (case-insensitive)
    test_cases = [
        (b'\x00\x01' + b'test\x00' + b'octet\x00', 3),  # RRQ for 'test' in octet mode
        (b'\x00\x01' + b'test\x00' + b'OCTET\x00', 3),  # RRQ for 'test' in OCTET mode (upper case)
        (b'\x00\x01' + b'test\x00' + b'Octet\x00', 3),  # RRQ for 'test' in Octet mode (mixed case)
        (b'\x00\x01' + b'test\x00' + b'NETASCII\x00', 3),  # RRQ for 'test' in NETASCII mode (mixed case)
        (b'\x00\x01' + b'test\x00' + b'netascii\x00', 3),  # RRQ for 'test' in netascii mode (mixed case)
    ]

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        for request, expected_opcode in test_cases:
            sock.sendto(request, server_address)
            data, dst_addr = sock.recvfrom(1024)
            opcode = struct.unpack('!H', data[:2])[0]
            assert opcode == expected_opcode
            exit_test(sock, dst_addr)
            time.sleep(1)


def test_timeout():
    server_address = ('127.0.0.1', 69)
    initial_request = b'\x00\x01' + b'test\x00' + b'octet\x00'  # RRQ for 'testfile.txt' in octet mode

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(initial_request, server_address)
        data, next_address = sock.recvfrom(1024)

        opcode, block_number = struct.unpack('!HH', data[:4])
        assert opcode == 3
        assert block_number == 1

        time.sleep(6)
        
        data, _ = sock.recvfrom(1024)
        opcode, block_number = struct.unpack('!HH', data[:4])
        assert opcode == 3
        assert block_number == 1

        send_ack(sock, block_number, next_address)




