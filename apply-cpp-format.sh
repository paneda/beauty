#!/bin/bash

# Variable to track if any files were formatted
formatted_files_count=0

format_directory() {
  local dir=$1
  
  # Use git ls-files to get tracked files (not in .gitignore)
  # and filter for C++ files only
  local files=$(git ls-files "$dir" | grep -E '\.cpp$|\.hpp$')
  
  if [ -z "$files" ]; then
    echo "No tracked C++ files found in $dir/"
    return
  fi
  
  echo "Formatting files in $dir/:"
  
  while IFS= read -r file; do
    # Only process if file exists (extra safety check)
    if [ -f "$file" ]; then
      echo "  ✓ $(basename "$file")"
      clang-format -i "$file"
      ((formatted_files_count++))
    fi
  done <<< "$files"
  
  echo "--------------------------------------------------------"
}

# Check if we're in a git repository
if ! git rev-parse --is-inside-work-tree > /dev/null 2>&1; then
  echo "Error: This script must be run from within a git repository."
  exit 1
fi

echo "Applying clang-format to tracked C++ files (ignoring .gitignored files)..."
echo "--------------------------------------------------------"

format_directory "src"
format_directory "examples"
format_directory "test"

# Summary
if [ "$formatted_files_count" -gt 0 ]; then
  echo "Formatting complete! $formatted_files_count files were formatted."
else
  echo "No files were formatted. Check if the directories exist and contain tracked C++ files."
fi
