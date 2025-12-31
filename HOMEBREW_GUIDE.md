# Homebrew Formula Setup Guide for Pixie Renderer

## TL;DR - Quick Setup (5 Steps)

```bash
# 1. Create GitHub repo: homebrew-pixie (public)
# 2. Create tap locally
mkdir -p ~/homebrew-pixie/Formula
cd ~/homebrew-pixie
git init
git remote add origin https://github.com/YOUR_USERNAME/homebrew-pixie.git

# 3. Create formula
curl -L https://github.com/YOUR_USERNAME/openrender/archive/refs/tags/v2.2.6.tar.gz | shasum -a 256
# Copy SHA256 output and create Formula/pixie.rb (see template below)

# 4. Test locally
brew install --build-from-source ./Formula/pixie.rb
brew test pixie

# 5. Publish
git add Formula/pixie.rb
git commit -m "Add Pixie formula"
git push -u origin main
```

Users install with: `brew install YOUR_USERNAME/pixie/pixie`

---

## Detailed Step-by-Step Guide

### Prerequisites

- ✅ Homebrew installed (`brew --version`)
- ✅ Git installed
- ✅ GitHub account
- ✅ Your openrender project on GitHub with a release tag

### Step 1: Create a GitHub Release (if you don't have one)

```bash
# In your openrender repository
git tag -a v2.2.6 -m "Release version 2.2.6"
git push origin v2.2.6
```

Then on GitHub:
1. Go to your repository → Releases
2. Click "Create a new release"
3. Select tag `v2.2.6`
4. Add release notes
5. Click "Publish release"

### Step 2: Create Homebrew Tap Repository

On GitHub:
1. Create new repository: `homebrew-pixie` (or `homebrew-rendering`, etc.)
2. Make it **public**
3. **Don't** initialize with README

### Step 3: Set Up Local Tap Directory

```bash
# Create directory
mkdir -p ~/homebrew-pixie
cd ~/homebrew-pixie

# Initialize git
git init
git remote add origin https://github.com/YOUR_USERNAME/homebrew-pixie.git

# Create Formula directory
mkdir Formula
```

### Step 4: Calculate SHA256 Hash

```bash
# Replace YOUR_USERNAME and v2.2.6 with your values
curl -L https://github.com/YOUR_USERNAME/openrender/archive/refs/tags/v2.2.6.tar.gz | shasum -a 256
```

Copy the SHA256 hash that's printed.

### Step 5: Create Formula File

Create `Formula/pixie.rb` with this content:

```ruby
class Pixie < Formula
  desc "RenderMan-compliant photorealistic renderer"
  homepage "https://github.com/YOUR_USERNAME/openrender"
  url "https://github.com/YOUR_USERNAME/openrender/archive/refs/tags/v2.2.6.tar.gz"
  sha256 "PASTE_SHA256_HERE"  # From Step 4
  license "LGPL-2.1-only"
  
  head "https://github.com/YOUR_USERNAME/openrender.git", branch: "master"

  depends_on "cmake" => :build
  depends_on "flex" => :build
  depends_on "bison" => :build
  depends_on "libtiff"
  depends_on "libpng"
  depends_on "zlib"
  
  # Optional dependencies
  depends_on "fltk" => :optional
  depends_on "openexr" => :optional
  depends_on "libx11" => :optional

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    system "#{bin}/rndr", "--help"
    system "#{bin}/sdrc", "--version"
    assert_predicate lib/"libri.dylib", :exist?
  end
end
```

### Step 6: Test Locally

```bash
# Install from local formula
brew install --build-from-source --verbose ./Formula/pixie.rb

# Run tests
brew test pixie

# Audit (check for issues)
brew audit --new-formula pixie

# Try the installed commands
rndr --help
sdrc --version
```

If it fails, check the error messages and adjust the formula.

### Step 7: Publish to GitHub

```bash
git add Formula/pixie.rb
git commit -m "Add Pixie 2.2.6 formula"
git branch -M main
git push -u origin main
```

### Step 8: Users Install

Now anyone can install with:

```bash
# Add your tap
brew tap YOUR_USERNAME/pixie

# Install
brew install pixie
```

Or in one command:
```bash
brew install YOUR_USERNAME/pixie/pixie
```

---

## Updating to New Versions

When you release v2.2.7:

```bash
# 1. Get new SHA256
curl -L https://github.com/YOUR_USERNAME/openrender/archive/refs/tags/v2.2.7.tar.gz | shasum -a 256

# 2. Edit Formula/pixie.rb
#    - Update url to v2.2.7.tar.gz
#    - Update sha256 with new hash

# 3. Test
brew uninstall pixie
brew install --build-from-source ./Formula/pixie.rb

# 4. Publish
git add Formula/pixie.rb
git commit -m "pixie: update to 2.2.7"
git push
```

Users upgrade with: `brew upgrade pixie`

---

## Common Issues & Solutions

### Issue: SHA256 mismatch
**Solution:** Recalculate SHA256 from the exact tarball URL in your formula

### Issue: CMake configuration fails
**Solution:** Check that all dependencies are available. Run with `--verbose` to see details

### Issue: Tests fail
**Solution:** Adjust the `test` block in your formula. Make sure executables exist and work

### Issue: Audit warnings
**Solution:** Run `brew audit --new-formula pixie` and fix reported issues

### Issue: Can't push to GitHub
**Solution:** Make sure you have permission. You may need to authenticate:
```bash
gh auth login  # If you have GitHub CLI
# Or use HTTPS/SSH authentication
```

---

## Advanced: Optional Dependencies

Users can install with/without optional features:

```bash
# With all features
brew install pixie --with-fltk --with-openexr --with-libx11

# Without GUI
brew install pixie --without-fltk
```

To handle this in your formula:

```ruby
def install
  args = std_cmake_args
  
  # Disable GUI if FLTK not requested
  args << "-DBUILD_SHOW=OFF" if build.without? "fltk"
  
  system "cmake", "-S", ".", "-B", "build", *args
  system "cmake", "--build", "build"
  system "cmake", "--install", "build"
end
```

---

## Files You'll Create

```
~/homebrew-pixie/
├── Formula/
│   └── pixie.rb          # Your formula
├── .git/                  # Git repository
└── README.md             # Optional: usage instructions
```

---

## Useful Commands

```bash
# Uninstall
brew uninstall pixie

# Reinstall
brew reinstall pixie

# Get info
brew info pixie

# Check dependencies
brew deps pixie

# See what files were installed
brew list pixie

# Edit formula
brew edit pixie
```

---

## Resources

- **Homebrew Formula Cookbook**: https://docs.brew.sh/Formula-Cookbook
- **Formula Class API**: https://rubydoc.brew.sh/Formula
- **Contributing Guide**: https://docs.brew.sh/How-To-Open-a-Homebrew-Pull-Request
- **Example Formulas**: Look in `/opt/homebrew/Library/Taps/homebrew/homebrew-core/Formula/`

---

## Need Help?

- Homebrew Discussions: https://github.com/Homebrew/discussions/discussions
- Formula Issues: https://github.com/Homebrew/homebrew-core/issues
- Your tap issues: Create issues in your homebrew-pixie repository

