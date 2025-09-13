#!/usr/bin/env python3
import requests

# This script tests the example server's handling of various POST requests.
# Start the server first with: build/examples/beauty_example 127.0.0.1 8080 www
def do_post_request(auth_token, payload, url="http://localhost:8080"):

    headers = {
        "Content-Type": "application/json",
        "Content-Length": str(len(payload)),
        "Connection": "close",  # Force connection close after each request
    }

    try:
        response = requests.post(url, data=payload, headers=headers, timeout=10)

        print(f"Status Code: {response.status_code}")
        print(f"Response Headers: {response.headers}")
        print(f"Response Body: {response.text}\n")

    except requests.exceptions.ConnectionError as e:
        print(f"Error: Connection closed by the server. This is expected for 4xx errors. Details: {e}\n")


if __name__ == "__main__":
    print("--- Starting Client Tests ---")
    print("This script makes various POST request in a rapid sequence.\n")

    print("---------------------------------------")
    print(f"Testing a posting a too large payload...")
    payload = "x" * 1024 * 1024 # 1M payload
    do_post_request("Bearer valid_token", payload=payload)

    print("---------------------------------------")
    print(f"Testing invalid json...")
    payload = "{\"name\": \"John Doe\" " # invalid json, missing closing brace
    do_post_request("Bearer valid_token", payload=payload, url="http://localhost:8080/api/users")

    print("---------------------------------------")
    print(f"Testing valid json...")
    payload = "{\"name\": \"John Doe\"}"
    do_post_request("Bearer valid_token", payload=payload, url="http://localhost:8080/api/users")

