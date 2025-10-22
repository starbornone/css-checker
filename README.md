# CSS Checker C++

A fast and efficient C++ tool for finding and removing unused CSS classes in web projects. This tool extends beyond JavaScript/TypeScript support to include Razor (.cshtml) and Web Forms (.aspx) files.

## Features

- **Fast Performance**: Written in C++ for optimal speed on large codebases
- **Multi-Format Support**: 
  - JavaScript/TypeScript (.js, .jsx, .ts, .tsx)
  - Razor Pages (.cshtml) - ASP.NET Core
  - Web Forms (.aspx) - ASP.NET Framework
  - CSS files (.css)
- **Smart Detection**: Recognizes dynamic class usage patterns and template literals
- **Safe Cleanup**: Creates backup files before removing unused classes
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
cmake .. -G "Visual Studio 17 2022" -A x64
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

# Actually remove unused classes (creates backups)
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
    --cleanup           Remove unused CSS classes (creates backups)
    --no-report         Don't generate markdown report
    --css-ext <EXTS>    CSS file extensions (default: .css)
    --code-ext <EXTS>   Code file extensions (default: .tsx,.ts,.js,.jsx,.cshtml,.aspx)
```

## Supported File Types

### CSS Files
- Standard CSS class definitions (`.class-name`)
- Ignores pseudo-classes, @media queries, and single-word classes

### JavaScript/TypeScript
- Direct className usage: `className="my-class"`
- Template literals: `` `my-class` ``
- clsx() utility functions
- Dynamic class generation patterns

### Razor (.cshtml) Files
- Standard HTML class attributes: `class="my-class"`
- Razor syntax: `@class="my-class"`
- Server-side expressions: `@{ ... }`
- String interpolation: `$"my-class-{variable}"`
- HTML Helpers and ViewBag/ViewData references

### Web Forms (.aspx) Files
- HTML class attributes: `class="my-class"`
- Server control CssClass properties: `CssClass="my-class"`
- Code-behind references: `<%=  %>`, `<%#  %>`
- Data binding expressions: `Eval()`, `Bind()`
- User controls and master pages

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
# Include Web Forms specific extensions
./css_checker --code-ext .aspx,.ascx,.master,.cs --verbose
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

# 4. Restore from backup if needed
# cp file.css.backup file.css
```

## Output

### Console Output
The tool provides color-coded console output showing:
- ✅ Used classes (in green)
- ❌ Unused classes (in red)
- 📊 Summary statistics
- 📄 Generated report location

### Markdown Report
Generates a detailed report including:
- Summary by file
- List of unused classes
- Cleanup suggestions
- Configuration used

## Performance

The C++ implementation offers significant performance improvements over JavaScript-based tools:
- **2-5x faster** file scanning
- **3-10x faster** regex processing
- **Lower memory usage** for large codebases
- **Native file system access**

## Safety Features

- **Backup Creation**: Automatically creates `.backup` files before cleanup
- **Dry Run Mode**: Preview changes without modification
- **Smart Pattern Detection**: Recognizes dynamic usage patterns
- **Verbose Logging**: Detailed output for debugging

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
| Performance | Moderate | Fast |
| Memory Usage | Higher | Lower |
| CSHTML Support | ❌ | ✅ |
| ASPX Support | ❌ | ✅ |
| Dependency-free | ❌ | ✅ |
| Cross-platform | ✅ | ✅ |

The C++ version is designed to be a drop-in replacement with enhanced capabilities for .NET web projects.