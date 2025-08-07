#!/bin/bash

# Variable to track if any files need formatting
needs_formatting_count=0

check_directory() {
  local dir=$1
  
  # Use git ls-files to get tracked files (not in .gitignore)
  # and filter for C++ files only
  local files=$(git ls-files "$dir" | grep -E '\.cpp$|\.hpp$')
  
  if [ -z "$files" ]; then
    echo "No tracked C++ files found in $dir/"
    return
  fi
  
  echo "Checking files in $dir/:"
  
  while IFS= read -r file; do
    # Only process if file exists (extra safety check)
    if [ -f "$file" ]; then
      # Check if formatting is needed using clang-format's built-in diff feature
      if ! clang-format --dry-run --Werror "$file" &>/dev/null; then
        echo "  ✗ $(basename "$file") needs formatting"
        ((needs_formatting_count++))
      else
        echo "  ✓ $(basename "$file")"
      fi
    fi
  done <<< "$files"
  
  echo "--------------------------------------------------------"
}


# Check if we're in a git repository
if ! git rev-parse --is-inside-work-tree > /dev/null 2>&1; then
  echo "Error: This script must be run from within a git repository."
  exit 1
fi

echo "Checking C++ files for formatting issues (ignoring .gitignored files)..."
echo "--------------------------------------------------------"

check_directory "src"
check_directory "examples"
check_directory "test"

# Summary
if [ "$needs_formatting_count" -gt 0 ]; then
  echo "Check complete! $needs_formatting_count files need formatting."
  exit 1  # Exit with non-zero status if files need formatting
else
  echo "All files are properly formatted."
  exit 0  # Exit with zero status if all files are formatted correctly
fi
