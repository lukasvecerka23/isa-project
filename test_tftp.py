import socket
import time
import struct
import pytest

server_address = ('127.0.0.1', 69)

def send_rrq(sock, filename, mode, server_address):
    rrq_packet = struct.pack('!H', 1)
    rrq_packet += filename
    rrq_packet += b'\x00'
    rrq_packet += mode
    rrq_packet += b'\x00'
    sock.sendto(rrq_packet, server_address)

def send_ack(sock, block_number, server_address):
    ack_packet = struct.pack('!HH', 4, block_number)
    sock.sendto(ack_packet, server_address)

def send_data(sock, block_number, data, server_address):
    data_packet = struct.pack('!HH', 3, block_number)
    data_packet += data
    sock.sendto(data_packet, server_address)

def exit_test(sock, server_address):
    exit_packet = b'\00\05\00\00\00'
    sock.sendto(exit_packet, server_address)


wrong_rrq_wrq_test_cases = [
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
        (b'\x00\x05\x00\x09\00', 4), # Wrong error code
        (b'\x00\x01test\x00octet\x00blksize\x00', 8),  # Missing value for blksize option
        (b'\x00\x01test\x00octet\x00blksize\x001024', 8),  # Missing null byte after blksize value
        (b'\x00\x01test\x00octet\x00blksize\x001024\x00timeout\x00', 8),  # Missing value for timeout option
        (b'\x00\x01test\x00octet\x00blksize\x001024\x00timeout\x0030', 8),  # Missing null byte after timeout value
        (b'\x00\x01test\x00octet\x00blksize\x001024\x00timeout\x0030\x00tsize\x00', 8),  # Missing value for tsize option
        (b'\x00\x01test\x00octet\x00blksize\x001024\x00timeout\x0030\x00tsize\x0010000', 8),  # Missing null byte after tsize value
        (b'\x00\x01test\x00octet\x00blksize\x001024\x00timeout\x0030\x00tsize\x0010000\x00option\x00', 8),  # Unknown option
        (b'\x00\x01test\x00octet\x00blksize\x001024\x00timeout\x0030\x00tsize\x0010000\x00option\x00value', 8),  # Missing null byte after unknown option value
        (b'\x00\x01test/filename\x00octet\x00', 1), # Filename with subdirectory
        (b'\x00\x01test\x00octet\x00blksize\x001024\x00blksize\x0030', 8),
    ]

@pytest.mark.parametrize('data, expected_error_code', wrong_rrq_wrq_test_cases)
def test_wrong_request(data, expected_error_code):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.sendto(data, server_address)
            data, _ = sock.recvfrom(1024)
            opcode, err_code = struct.unpack('!HH', data[:4])
            assert opcode == 5
            assert err_code == expected_error_code
            time.sleep(1)

# Test cases for RRQ and WRQ with different modes (case-insensitive)
correct_rrq_wrq_test_cases = [
    (b'\x00\x01' + b'test\x00' + b'octet\x00', 3),  # RRQ for 'test' in octet mode
    (b'\x00\x01' + b'test\x00' + b'OCTET\x00', 3),  # RRQ for 'test' in OCTET mode (upper case)
    (b'\x00\x01' + b'test\x00' + b'Octet\x00', 3),  # RRQ for 'test' in Octet mode (mixed case)
    (b'\x00\x01' + b'test\x00' + b'NETASCII\x00', 3),  # RRQ for 'test' in NETASCII mode (mixed case)
    (b'\x00\x01' + b'test\x00' + b'netascii\x00', 3),  # RRQ for 'test' in netascii mode (mixed case)
]

@pytest.mark.parametrize('data,expected_opcode', correct_rrq_wrq_test_cases)
def test_correct_rrq_wrq(data, expected_opcode):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.sendto(data, server_address)
            data, dst_addr = sock.recvfrom(1024)
            opcode = struct.unpack('!H', data[:2])[0]
            assert opcode == expected_opcode
            exit_test(sock, dst_addr)
            time.sleep(1)

correct_options_test_cases = [
    (b'\x00\x01' + b'test\x00' + b'octet\x00' + b'blksize\x001024\x00', 6),  # RRQ for 'test' in octet mode with blksize option
    (b'\x00\x01' + b'test\x00' + b'octet\x00' + b'blksize\x001024\x00' + b'timeout\x0030\x00', 6),  # RRQ for 'test' in octet mode with blksize and timeout options
    (b'\x00\x01' + b'test\x00' + b'octet\x00' + b'blksize\x001024\x00' + b'timeout\x0030\x00' + b'tsize\x000\x00', 6),  # RRQ for 'test' in octet mode with blksize, timeout and tsize options
    (b'\x00\x01' + b'test\x00' + b'octet\x00' + b'blksize\x001024\x00' + b'timeout\x0030\x00' + b'tsize\x000\x00' + b'option\x00value\x00', 6),  # RRQ for 'test' in octet mode with blksize, timeout, tsize and unknown options
    (b'\x00\x01' + b'test\x00' + b'octet\x00' + b'BLKSIZE\x001024\x00', 6),  # RRQ for 'test' in octet mode with BLKSIZE option
    (b'\x00\x01' + b'test\x00' + b'octet\x00' + b'TIMEOUT\x00255\x00', 6),  # RRQ for 'test' in octet mode with TIMEOUT option
    (b'\x00\x01' + b'test\x00' + b'octet\x00' + b'TSIZE\x000\x00', 6),  # RRQ for 'test' in octet mode with TSIZE option
    (b'\x00\x01test\x00octet\x00blksize\x0065467\x00', 6), # Block size exceed 65464 but server should accept it and set it to 65464
]

@pytest.mark.parametrize('data,expected_opcode', correct_options_test_cases)
def test_correct_options(data, expected_opcode):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.sendto(data, server_address)
            data, dst_addr = sock.recvfrom(1024)
            opcode = struct.unpack('!H', data[:2])[0]
            assert opcode == expected_opcode
            exit_test(sock, dst_addr)
            time.sleep(1)


options_out_of_range_test_cases = [
    (b'\x00\x01test\x00octet\x00timeout\x00256\x00', 3), # Timeout value too big
    (b'\x00\x01test\x00octet\x00timeout\x00-1\x00', 3), # Timeout under 0
    (b'\x00\x01test\x00octet\x00blksize\x007\x00', 3), # Block size value under 8
    (b'\x00\x01test\x00octet\x00tsize\x00-1\x00', 3), # Tsize under 0
    (b'\x00\x01test\x00octet\x00tsize\x004290183241\x00', 3), # 65464*65535 + 1 Tsize too big
    (b'\x00\x01test\x00octet\x00tsize\x0030\x00', 3), # Read request with tsize not 0
    (b'\x00\x01test\x00octet\x00blksize\x00aaaa\x00', 3), # Block size not a number
]

@pytest.mark.parametrize('data,expected_opcode', options_out_of_range_test_cases)
def test_options_out_of_range(data, expected_opcode):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.sendto(data, server_address)
            data, dst_addr = sock.recvfrom(1024)
            opcode = struct.unpack('!H', data[:2])[0]
            assert opcode == expected_opcode
            exit_test(sock, dst_addr)
            time.sleep(1)

"""
Client send RRQ, obtain Data packet with #1 wait for 6 seconds obtain another
Data packet with #1 because server timeouted and send ACK with #1
"""
def test_timeout():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        send_rrq(sock, b'test', b'octet', server_address)
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

        exit_test(sock, next_address)

# Client send RRQ, obtain Data packet with #1 and send ACK with #2
def test_wrong_ack():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        send_rrq(sock, b'test', b'octet', server_address)
        data, next_address = sock.recvfrom(1024)

        opcode, block_number = struct.unpack('!HH', data[:4])
        assert opcode == 3
        assert block_number == 1

        send_ack(sock, block_number + 1, next_address)

        data, _ = sock.recvfrom(1024)
        opcode, err_code = struct.unpack('!HH', data[:4])
        assert opcode == 5
        assert err_code == 4

"""
Client send initial RRQ packet, obtain Data packet with #1, from another 
socket is send ACK with #1 and obtain Error packet with #5
"""
def test_wrong_TID():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        send_rrq(sock, b'test', b'octet', server_address)
        data, next_address = sock.recvfrom(1024)

        opcode, block_number = struct.unpack('!HH', data[:4])
        assert opcode == 3
        assert block_number == 1

        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock2:
            send_ack(sock2, block_number, next_address)

            data, _ = sock2.recvfrom(1024)
            opcode, err_code = struct.unpack('!HH', data[:4])
            assert opcode == 5
            assert err_code == 5

        
        send_ack(sock, block_number, next_address)

        exit_test(sock, next_address)


def test_wrong_packet():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(b'\x00\x01test\x00octet\x00', server_address)
        data, next_address = sock.recvfrom(1024)

        opcode, block_number = struct.unpack('!HH', data[:4])
        assert opcode == 3
        assert block_number == 1

        send_data(sock, block_number, b'1234567890', next_address)

        data, _ = sock.recvfrom(1024)
        opcode, err_code = struct.unpack('!HH', data[:4])
        assert opcode == 5
        assert err_code == 4

def test_exp_backoff():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        send_rrq(sock, b'test', b'octet', server_address)
        data, next_address = sock.recvfrom(1024)

        opcode, block_number = struct.unpack('!HH', data[:4])
        assert opcode == 3
        assert block_number == 1

        time.sleep(6)
        
        data, _ = sock.recvfrom(1024)
        opcode, block_number = struct.unpack('!HH', data[:4])
        assert opcode == 3
        assert block_number == 1

        time.sleep(11)

        data, _ = sock.recvfrom(1024)
        opcode, block_number = struct.unpack('!HH', data[:4])
        assert opcode == 3
        assert block_number == 1

        send_ack(sock, block_number, next_address)

        exit_test(sock, next_address)

def test_timeout_option():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        initial_data = b'\x00\x01' + b'test\x00' + b'octet\x00' + b'timeout\x0010\x00'
        sock.sendto(initial_data, server_address)
        data, next_address = sock.recvfrom(1024)

        opcode = struct.unpack('!H', data[:2])[0]
        assert opcode == 6

        sock.settimeout(6)
        
        try:
            data, _ = sock.recvfrom(1024)
            opcode = struct.unpack('!H', data[:2])[0]
        except socket.timeout:
            data = None
            opcode = None
        
        assert opcode == 6

        send_ack(sock, 0, next_address)

        data, _ = sock.recvfrom(1024)
        opcode, block_number = struct.unpack('!HH', data[:4])
        assert opcode == 3
        assert block_number == 1

        sock.settimeout(3)
        try:
            data, _ = sock.recvfrom(1024)
        except socket.timeout:
            data = None
        
        assert not data 

        sock.settimeout(8)
        try:
            data, _ = sock.recvfrom(1024)
            opcode, block_number = struct.unpack('!HH', data[:4])
        except socket.timeout:
            data = None
            opcode = None
            block_number = None

        assert opcode == 3
        assert block_number == 1

        send_ack(sock, block_number, next_address)

        exit_test(sock, next_address)

def test_exceed_blksize_option():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        initial_data = b'\x00\x02' + b'newtest\x00' + b'octet\x00' + b'blksize\x0010\x00'
        sock.sendto(initial_data, server_address)
        data, next_address = sock.recvfrom(1024)

        opcode = struct.unpack('!H', data[:2])[0]
        assert opcode == 6

        # send data which has size more than blksize
        data = b'\x00\x03\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01'
        sock.sendto(data, next_address)

        data, _ = sock.recvfrom(1024)
        opcode, block_number = struct.unpack('!HH', data[:4])
        assert opcode == 5
        assert block_number == 4