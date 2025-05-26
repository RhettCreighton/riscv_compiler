#!/bin/bash

# Script to add SPDX license headers to all source files
# Copyright 2025 Rhett Creighton

HEADER_C="/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

"

HEADER_SH="#!/bin/bash
# SPDX-FileCopyrightText: 2025 Rhett Creighton
# SPDX-License-Identifier: Apache-2.0

"

# Function to add header to C/H files
add_c_header() {
    local file="$1"
    
    # Skip if already has SPDX header
    if grep -q "SPDX-License-Identifier" "$file"; then
        echo "Skipping $file (already has SPDX header)"
        return
    fi
    
    # Create temp file with header
    echo "$HEADER_C" > "$file.tmp"
    cat "$file" >> "$file.tmp"
    mv "$file.tmp" "$file"
    echo "Added header to $file"
}

# Function to add header to shell scripts
add_sh_header() {
    local file="$1"
    
    # Skip if already has SPDX header
    if grep -q "SPDX-License-Identifier" "$file"; then
        echo "Skipping $file (already has SPDX header)"
        return
    fi
    
    # Check if file starts with shebang
    if head -n1 "$file" | grep -q "^#!/"; then
        # Save shebang
        head -n1 "$file" > "$file.tmp"
        echo "# SPDX-FileCopyrightText: 2025 Rhett Creighton" >> "$file.tmp"
        echo "# SPDX-License-Identifier: Apache-2.0" >> "$file.tmp"
        echo "" >> "$file.tmp"
        tail -n +2 "$file" >> "$file.tmp"
    else
        # No shebang, add full header
        echo "$HEADER_SH" > "$file.tmp"
        cat "$file" >> "$file.tmp"
    fi
    
    mv "$file.tmp" "$file"
    echo "Added header to $file"
}

# Process all C files
echo "Processing C files..."
find src/ include/ tests/ examples/ -name "*.c" -type f | while read -r file; do
    add_c_header "$file"
done

# Process all H files
echo "Processing H files..."
find src/ include/ tests/ examples/ -name "*.h" -type f | while read -r file; do
    add_c_header "$file"
done

# Process all shell scripts
echo "Processing shell scripts..."
find scripts/ -name "*.sh" -type f | while read -r file; do
    add_sh_header "$file"
done

# Also process CMakeLists.txt
echo "Processing CMakeLists.txt..."
if ! grep -q "SPDX-License-Identifier" "CMakeLists.txt"; then
    {
        echo "# SPDX-FileCopyrightText: 2025 Rhett Creighton"
        echo "# SPDX-License-Identifier: Apache-2.0"
        echo ""
        cat "CMakeLists.txt"
    } > "CMakeLists.txt.tmp"
    mv "CMakeLists.txt.tmp" "CMakeLists.txt"
    echo "Added header to CMakeLists.txt"
fi

echo "License headers added successfully!"