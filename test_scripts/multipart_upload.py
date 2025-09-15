#!/usr/bin/env python3
import requests
import io

def upload_virtual_files(files_to_upload, headers={}, url="http://localhost:8080"):
    """
    files_to_upload: list of tuples (file_name, file_content)
    file_content should be bytes
    """
    files = {}
    for idx, (file_name, file_content) in enumerate(files_to_upload, 1):
        # Use a unique field name for each file (e.g., file1, file2, ...)
        field_name = f'file{idx}'
        files[field_name] = (file_name, io.BytesIO(file_content))
    response = requests.post(url, files=files, headers=headers)
    print(f"Status Code: {response.status_code}")
    print(f"Response Headers: {response.headers}")
    print(f"Response Body: {response.text}\n")

if __name__ == "__main__":
    print("--- Starting Client Tests ---")
    print("This script makes various POST multipart requests in a rapid sequence.\n")
    small_content = b"Hello, this is a small virtual file."
    large_content = b"A" * (10 * 1024 * 1024)  # 10MB
    file1_content = b"B" * (128)
    file2_content = b"C" * (256)
    file3_content = b"D" * (512)

    print("---------------------------------------")
    print(f"Testing uploading a small virtual file...")
    files_to_upload = [("small.txt", small_content)]
    upload_virtual_files(files_to_upload)

    print("---------------------------------------")
    print(f"Testing uploading a large virtual file...")
    files_to_upload = [("large.bin", large_content)]
    upload_virtual_files(files_to_upload)

    print("---------------------------------------")
    print(f"Testing uploading an empty virtual file...")
    files_to_upload = [("empty.txt", b"")]
    upload_virtual_files(files_to_upload)

    print("---------------------------------------")
    print(f"Testing uploading a virtual file with special characters in the name...")
    files_to_upload = [("spécial_文件.txt", small_content)]
    upload_virtual_files(files_to_upload)

    print("---------------------------------------")
    print(f"Testing uploading multiple virtual files in one request...")
    files_to_upload = [
        ("file1.bin", file1_content),
        ("file2.bin", file2_content),
        ("file3.bin", file3_content)
    ]
    upload_virtual_files(files_to_upload)
