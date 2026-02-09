#!/bin/bash
set -e

echo "=================================================="
echo "Homebrew Formula Quick-Start for openRender Renderer"
echo "=================================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Step 1: Check prerequisites
echo -e "${BLUE}Step 1: Checking prerequisites...${NC}"
if ! command -v brew &> /dev/null; then
    echo "❌ Homebrew not found. Install from https://brew.sh"
    exit 1
fi
echo "✅ Homebrew installed"

if ! command -v git &> /dev/null; then
    echo "❌ Git not found. Please install git"
    exit 1
fi
echo "✅ Git installed"
echo ""

# Step 2: GitHub setup instructions
echo -e "${BLUE}Step 2: GitHub Repository Setup${NC}"
echo "Before continuing, you need to:"
echo "  1. Go to https://github.com/new"
echo "  2. Create a repository named: ${GREEN}homebrew-openrender${NC}"
echo "     (Or homebrew-<yourname>, following the homebrew-* pattern)"
echo "  3. Make it ${GREEN}public${NC}"
echo "  4. Don't initialize with README"
echo ""
read -p "Press Enter once you've created the repository..."
echo ""

# Step 3: Get repository details
echo -e "${BLUE}Step 3: Repository Details${NC}"
read -p "Enter your GitHub username: " GITHUB_USER
read -p "Enter your tap name (e.g., 'openrender'): " TAP_NAME
REPO_NAME="homebrew-${TAP_NAME}"
echo ""

# Step 4: Clone or create tap directory
echo -e "${BLUE}Step 4: Setting up local tap directory...${NC}"
TAP_DIR="${HOME}/homebrew-${TAP_NAME}"

if [ -d "$TAP_DIR" ]; then
    echo "⚠️  Directory ${TAP_DIR} already exists"
    read -p "Remove and recreate it? (y/n): " RECREATE
    if [ "$RECREATE" = "y" ]; then
        rm -rf "$TAP_DIR"
        echo "✅ Removed existing directory"
    else
        echo "Using existing directory..."
    fi
fi

if [ ! -d "$TAP_DIR" ]; then
    git clone "https://github.com/${GITHUB_USER}/${REPO_NAME}.git" "$TAP_DIR" 2>/dev/null || {
        mkdir -p "$TAP_DIR"
        cd "$TAP_DIR"
        git init
        git remote add origin "https://github.com/${GITHUB_USER}/${REPO_NAME}.git"
        echo "✅ Created new tap directory"
    }
else
    cd "$TAP_DIR"
    echo "✅ Using existing tap directory"
fi

cd "$TAP_DIR"
mkdir -p Formula
echo ""

# Step 5: Create the formula
echo -e "${BLUE}Step 5: Creating openRender formula...${NC}"
read -p "Enter your openrender repository URL (e.g., https://github.com/${GITHUB_USER}/openrender): " REPO_URL
read -p "Enter the version tag (e.g., v2.2.6): " VERSION
echo ""

# Calculate SHA256
echo "Calculating SHA256 hash of release tarball..."
TARBALL_URL="${REPO_URL}/archive/refs/tags/${VERSION}.tar.gz"
echo "Downloading: ${TARBALL_URL}"

SHA256=$(curl -sL "$TARBALL_URL" | shasum -a 256 | awk '{print $1}')
if [ -z "$SHA256" ]; then
    echo "❌ Failed to download or calculate SHA256"
    echo "You can manually calculate it later with:"
    echo "  curl -L ${TARBALL_URL} | shasum -a 256"
    SHA256="REPLACE_WITH_ACTUAL_SHA256"
else
    echo "✅ SHA256: ${SHA256}"
fi
echo ""

# Create the formula file
echo -e "${BLUE}Creating Formula/openrender.rb...${NC}"
cat > Formula/openrender.rb << FORMULA
class OpenRender < Formula
  desc "RenderMan-compliant photorealistic renderer"
  homepage "${REPO_URL}"
  url "${TARBALL_URL}"
  sha256 "${SHA256}"
  license "LGPL-2.1-only"
  
  head "${REPO_URL}.git", branch: "master"

  depends_on "cmake" => :build
  depends_on "flex" => :build
  depends_on "bison" => :build
  depends_on "libtiff"
  depends_on "libpng"
  depends_on "zlib"
  
  # Optional dependencies for additional features
  depends_on "fltk" => :optional      # GUI viewer
  depends_on "openexr" => :optional   # OpenEXR display driver
  depends_on "libx11" => :optional    # X11 framebuffer

  def install
    # Build with CMake
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    # Verify executables are installed and working
    system "#{bin}/orender", "--help"
    system "#{bin}/oshader", "--version"
    system "#{bin}/otexmake", "--help"
    
    # Check that libraries are installed
    assert_predicate lib/"libri.dylib", :exist?
    assert_predicate lib/"libsdr.dylib", :exist?
  end
end
FORMULA

echo "✅ Formula created at Formula/openrender.rb"
echo ""

# Step 6: Test the formula locally
echo -e "${BLUE}Step 6: Testing formula locally...${NC}"
echo "To test the formula before publishing:"
echo ""
echo "  cd ${TAP_DIR}"
echo "  brew install --build-from-source --verbose ./Formula/openrender.rb"
echo "  brew test openrender"
echo "  brew audit --new-formula openrender"
echo ""
read -p "Would you like to test now? (y/n): " TEST_NOW

if [ "$TEST_NOW" = "y" ]; then
    echo "Installing openrender from formula..."
    brew install --build-from-source --verbose ./Formula/openrender.rb || {
        echo "❌ Installation failed. Check the output above for errors."
        exit 1
    }

    echo ""
    echo "Running tests..."
    brew test openrender || {
        echo "⚠️  Tests failed. You may need to update the test block in the formula."
    }

    echo ""
    echo "Running audit..."
    brew audit --new-formula openrender || {
        echo "⚠️  Audit found issues. Review them before publishing."
    }
fi
echo ""

# Step 7: Publish to GitHub
echo -e "${BLUE}Step 7: Publishing to GitHub...${NC}"
echo "Your formula is ready to publish!"
echo ""
echo "Commands to publish:"
echo -e "${GREEN}"
echo "  cd ${TAP_DIR}"
echo "  git add Formula/openrender.rb"
echo "  git commit -m 'Add openRender ${VERSION} formula'"
echo "  git branch -M main"
echo "  git push -u origin main"
echo -e "${NC}"
read -p "Would you like to commit and push now? (y/n): " PUBLISH_NOW

if [ "$PUBLISH_NOW" = "y" ]; then
    git add Formula/openrender.rb
    git commit -m "Add openRender ${VERSION} formula"
    git branch -M main
    git push -u origin main || {
        echo "❌ Push failed. You may need to authenticate with GitHub."
        echo "Try running: git push -u origin main"
        exit 1
    }
    echo "✅ Formula published to GitHub!"
fi
echo ""

# Step 8: Usage instructions
echo -e "${GREEN}=================================================="
echo "🎉 Setup Complete!"
echo "==================================================${NC}"
echo ""
echo "Users can now install openRender with:"
echo -e "${BLUE}"
echo "  # Add your tap"
echo "  brew tap ${GITHUB_USER}/${TAP_NAME}"
echo ""
echo "  # Install openrender"
echo "  brew install openrender"
echo ""
echo "  # Or in one command:"
echo "  brew install ${GITHUB_USER}/${TAP_NAME}/openrender"
echo -e "${NC}"
echo ""
echo "To update the formula in the future:"
echo "  1. Create a new release tag on GitHub"
echo "  2. Update Formula/openrender.rb with new version and SHA256"
echo "  3. git commit -am 'openrender: update to <new-version>'"
echo "  4. git push"
echo ""
echo "For more information:"
echo "  - Homebrew Docs: https://docs.brew.sh/Formula-Cookbook"
echo "  - Your tap: https://github.com/${GITHUB_USER}/${REPO_NAME}"
echo ""
