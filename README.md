# CSS Checker C++

A fast and efficient C++ tool for finding and removing unused CSS classes in web projects. This tool extends beyond JavaScript/TypeScript support to include Razor (.cshtml) and Web Forms (.aspx) files.

## Features

- **Single pass**: Each code file is read once, then class names are looked up in memory
- **Multi-Format Support**: 
  - JavaScript/TypeScript (.js, .jsx, .ts, .tsx)
  - Razor Pages (.cshtml) - ASP.NET Core
  - Web Forms (.aspx) - ASP.NET Framework
  - CSS files (.css)
- **Smart Detection**: Reads each code file once and matches `class`, `className`, and `CssClass` attributes, plus `clsx` / `cn` / `classnames` and `classList` calls
- **Safe Cleanup**: Keeps the first `.backup`, writes the cleaned file beside it, and replaces the stylesheet only after that write succeeds
- **Detailed Reporting**: Generates markdown reports with comprehensive analysis
- **Flexible Configuration**: Customizable file extensions and source directories

## Installation

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2019+)
- CMake 3.15+

### Building from Source

```bash
# Clone or navigate to the project directory
cd css-checker

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the project
cmake --build . --config Release

# The executable will be in build/bin/css_checker (or build/bin/Release/css_checker.exe on Windows)
```

### Windows (Visual Studio)

```powershell
# In PowerShell
cd css-checker
mkdir build
cd build
cmake .. -A x64
cmake --build . --config Release
```

### Linux/macOS

```bash
cd css-checker
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Usage

### Basic Usage

```bash
# Analyze current directory's src folder
./css_checker

# Analyze specific directory
./css_checker /path/to/your/project

# Show detailed output
./css_checker --verbose

# Preview what would be cleaned (no changes made)
./css_checker --dry-run

# Remove exact-selector rules for unused classes (keeps the first .backup)
./css_checker --cleanup
```

### Advanced Options

```bash
# Custom source directory
./css_checker --src webapp

# Custom report file
./css_checker --report my-css-report.md

# Custom file extensions
./css_checker --code-ext .ts,.tsx,.vue,.svelte

# Combine options
./css_checker --src frontend --verbose --cleanup --report detailed-report.md
```

### Complete Options

```
OPTIONS:
    -h, --help          Show help message
    -v, --version       Show version information
    -s, --src <DIR>     Source directory to scan (default: src)
    -r, --report <FILE> Report file path (default: unused-css-report.md)
    --verbose           Show detailed output
    --dry-run           Show what would be done without making changes
    --cleanup           Remove unused CSS classes (keeps the first .backup)
    --no-report         Don't generate markdown report
    --css-ext <EXTS>    CSS file extensions (default: .css)
    --code-ext <EXTS>   Code file extensions (default: .tsx,.ts,.js,.jsx,.cshtml,.aspx)
    --ignore <REGEX>    Ignore class names matched by REGEX (repeatable)
```

## Supported File Types

CSS class names are read from `.css` files (comments and strings are skipped). Every configured code extension is scanned the same way:

- `class`, `className`, and `CssClass` attributes, including `@class`
- `clsx(...)`, `cn(...)`, `classnames(...)`
- `classList.add` / `toggle` / `contains` / `remove`
- A quoted string that is exactly one class token, such as `"lonely"`
- `@apply`, `@extend`, and `composes` in stylesheets. The selector that defines a class is not a use
- A computed class value, such as `class="@Model.Css"`, `CssClass="<%# Item.Css %>"`, `className={styles.btn}`, or `:class="itemClass"`. Names without a literal use are reported as dynamic and are not deleted. A string literal in the same expression still counts

`--ignore` takes a regular expression and can be repeated. `^col-` ignores `col-6` and does not ignore `color`. `^row$` ignores `row` and does not ignore `row-gap`.

Add extensions with `--code-ext` when a project uses Vue, Svelte, HTML, or C# files. A `class` or `className` attribute in those files is enough.

A template prefix such as `icon--${name}`, or a C# interpolated string with the same shape, marks `icon--*` as dynamic. Dynamic classes are reported and are not deleted. `node_modules`, `bin`, `obj`, and `.git` are not scanned.

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | No unused class has a selector that is exactly that class |
| 1 | An unused class still has an exact selector |
| 2 | Bad arguments, the files could not be analyzed, or cleanup failed |

## Examples

### Basic Project Analysis

```bash
# Analyze a typical ASP.NET Core project
./css_checker --src wwwroot

# Analyze with verbose output
./css_checker --src wwwroot --verbose
```

### Web Forms Project

```bash
# Include extra code files. Detection is the same for every extension.
./css_checker --code-ext .aspx,.ascx,.master,.cs,.vue,.svelte --verbose
```

### Mixed Project (React + ASP.NET)

```bash
# Analyze both frontend and backend
./css_checker --code-ext .tsx,.ts,.js,.jsx,.cshtml,.cs --src ClientApp
```

### Cleanup Workflow

```bash
# 1. First, see what would be removed
./css_checker --dry-run

# 2. Review the generated report
cat unused-css-report.md

# 3. Perform the actual cleanup
./css_checker --cleanup

# 4. Restore from the first backup if needed
# cp file.css.backup file.css
#
# Cleanup deletes a rule only when a selector is exactly the unused class.
# `.used .unused` and `.used.unused` are left unchanged.
# An existing `.backup` is not overwritten.
```

## Output

### Console Output
On a terminal that supports color, the tool shows:
- Unused classes
- Dynamic classes that were left in place
- Summary counts
- The report path

`--verbose` also lists each used class. Color is omitted when stdout is not a terminal.

### Markdown Report
Generates a detailed report including:
- Summary by file
- List of unused classes
- Cleanup suggestions
- Configuration used

## Performance

Each code file is read once and turned into a class-name index. Class lookup after that does not re-read or recompile patterns. There is no benchmark suite in this repository, so this README does not claim a speedup over other tools.

## Safety Features

- **Backup Creation**: The first `.backup` is kept; later cleanups do not replace it
- **Atomic Replace**: Cleaned CSS is written to a temporary file and then moved into place
- **Dry Run Mode**: Preview changes without modification
- **Conservative Edits**: Compound selectors and unclosed rules are left unchanged
- **Dynamic Classes**: Prefixes such as `icon--${name}` are reported and not deleted
- **Empty Code Scan**: `--cleanup` is skipped when no code files were found, so a bad path cannot delete every class

## Tests

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Comparison with JavaScript Version

| Feature | JavaScript | C++ |
|---------|------------|-----|
| Runtime dependencies | Required | None |
| CSHTML Support | No | Yes |
| ASPX Support | No | Yes |
| Dependency-free | No | Yes |
| Cross-platform | Yes | Yes |

The C++ version is designed to be a drop-in replacement with enhanced capabilities for .NET web projects.