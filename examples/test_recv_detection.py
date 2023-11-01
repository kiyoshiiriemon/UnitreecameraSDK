import socket

def parse_message(message):
    tokens = message.split(',')
    camera_name = tokens[0]
    object_label = tokens[1]
    confidence = float(tokens[2])
    bbox = tuple(map(float, tokens[3:]))
    return camera_name, object_label, confidence, bbox

def receive_udp_message(ip_address, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((ip_address, port))
    while True:
        data, addr = sock.recvfrom(1024)
        message = data.decode('utf-8')
        camera_name, object_label, confidence, bbox = parse_message(message)
        print(f"Camera: {camera_name}")
        print(f"Object: {object_label}")
        print(f"Confidence: {confidence}")
        print(f"Bbox: {bbox}")
        print()

if __name__ == "__main__":
    ip_address = '127.0.0.1'
    port = 12346
    receive_udp_message(ip_address, port)

