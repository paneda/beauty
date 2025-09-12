import requests

# This script tests the example server's handling of the Expect: 100-continue header
# Start the server first with: build/examples/beauty_example 127.0.0.1 8080 www
def test_post_with_expect(auth_token, payload, url="http://localhost:8080"):

    headers = {
        "Content-Type": "text/plain",
        "Content-Length": str(len(payload)),
        "Expect": "100-continue",
        "Authorization": auth_token,
    }

    try:
        response = requests.post(url, data=payload, headers=headers, timeout=10)

        print(f"Status Code: {response.status_code}")
        print(f"Response Headers: {response.headers}")
        print(f"Response Body: {response.text}\n")

    except requests.exceptions.ConnectionError as e:
        print(f"Error: Connection closed by the server. This is expected for 4xx errors. Details: {e}\n")


if __name__ == "__main__":
    server_url = "http://localhost:8080"
    print("--- Starting 100 Continue Client Tests ---")
    print("This script simulates various POST request using the Expect header.\n")

    print("---------------------------------------")
    print(f"Testing with valid Authorization and large payload...")
    payload = "x" * 1024 * 1024 # 1M payload
    test_post_with_expect("Bearer valid_token", payload=payload)

    print("---------------------------------------")
    print(f"Testing with payload within limits but invalid Authorization...")
    payload = "x" * 1024 # within 1024 bytes limit in example server
    test_post_with_expect("Bearer invalid_token", payload=payload)

