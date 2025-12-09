#!/bin/bash

# =============================================================================
# 🚀 GitHub v1.0.0 Ultra Ethernet Protocol Stack Release Script
# =============================================================================
#
# This script automates the GitHub release process for the world's first
# Ultra Ethernet protocol stack open source implementation.
#
# Usage: ./github-release-script.sh YOUR_GITHUB_USERNAME
# =============================================================================

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Header
echo -e "${PURPLE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${PURPLE}║  🚀 GitHub v1.0.0 Ultra Ethernet Release Script            ║${NC}"
echo -e "${PURPLE}║  World's First Ultra Ethernet Protocol Stack            ║${NC}"
echo -e "${PURPLE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check for GitHub username
if [ "$#" -ne 1 ]; then
    echo -e "${RED}❌ Error: Please provide your GitHub username${NC}"
    echo -e "${YELLOW}Usage: $0 YOUR_GITHUB_USERNAME${NC}"
    exit 1
fi

GITHUB_USERNAME="$1"
REPO_NAME="soft-ue-ns3"
REPO_URL="https://github.com/${GITHUB_USERNAME}/${REPO_NAME}.git"

echo -e "${CYAN}📋 Release Configuration:${NC}"
echo -e "   Repository: ${REPO_URL}"
echo -e "   Username:   ${GITHUB_USERNAME}"
echo ""

# Step 1: Verify current status
echo -e "${BLUE}🔍 Step 1: Verifying project status...${NC}"
echo ""

# Check if we're on the right branch
CURRENT_BRANCH=$(git branch --show-current)
if [ "$CURRENT_BRANCH" != "soft-ue-integration" ]; then
    echo -e "${RED}❌ Error: Not on the correct branch${NC}"
    echo -e "${YELLOW}Current branch: ${CURRENT_BRANCH}${NC}"
    echo -e "${YELLOW}Expected branch: soft-ue-integration${NC}"
    exit 1
fi

# Check for uncommitted changes
if [ -n "$(git status --porcelain)" ]; then
    echo -e "${YELLOW}⚠️  Warning: There are uncommitted changes${NC}"
    echo "Uncommitted changes:"
    git status --porcelain
    echo ""
    read -p "Do you want to continue anyway? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${RED}❌ Aborting release${NC}"
        exit 1
    fi
fi

# Check if core files exist
echo -e "${CYAN}📁 Checking core files...${NC}"
CORE_FILES=("README.md" "LICENSE" "CONTRIBUTING.md" "CHANGELOG.md")
for file in "${CORE_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        echo -e "${RED}❌ Missing core file: $file${NC}"
        exit 1
    else
        echo -e "   ✅ $file ($(stat -c%s "$file") bytes)"
    fi
done

# Check for compiled library
if [ ! -f "build/lib/libns3.44-soft-ue-default.so" ]; then
    echo -e "${RED}❌ Missing compiled library: build/lib/libns3.44-soft-ue-default.so${NC}"
    exit 1
else
    LIB_SIZE=$(stat -c%s "build/lib/libns3.44-soft-ue-default.so")
    echo -e "   ✅ libns3.44-soft-ue-default.so ($(($LIB_SIZE / 1024 / 1024))MB)"
fi

# Check for v1.0.0 tag
if ! git rev-parse --verify "v1.0.0" >/dev/null 2>&1; then
    echo -e "${RED}❌ Missing v1.0.0 tag${NC}"
    exit 1
else
    echo -e "   ✅ v1.0.0 tag exists"
fi

echo ""
echo -e "${GREEN}✅ All checks passed!${NC}"
echo ""

# Step 2: Update remote URL
echo -e "${BLUE}🔄 Step 2: Updating remote repository URL...${NC}"
git remote set-url origin "$REPO_URL"
echo -e "   Remote URL updated to: ${REPO_URL}"
echo ""

# Step 3: Push to GitHub
echo -e "${BLUE}🚀 Step 3: Pushing to GitHub...${NC}"
echo ""

# Push main branch
echo -e "${CYAN}   Pushing soft-ue-integration branch...${NC}"
if git push origin soft-ue-integration; then
    echo -e "   ${GREEN}✅ Branch pushed successfully${NC}"
else
    echo -e "${RED}❌ Failed to push branch${NC}"
    exit 1
fi

# Push tags
echo -e "${CYAN}   Pushing tags...${NC}"
if git push origin --tags; then
    echo -e "   ${GREEN}✅ Tags pushed successfully${NC}"
else
    echo -e "${RED}❌ Failed to push tags${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}🎉 Code successfully pushed to GitHub!${NC}"
echo ""

# Step 4: Instructions for creating GitHub Release
echo -e "${BLUE}📋 Step 4: GitHub Release Creation Instructions${NC}"
echo ""
echo -e "${CYAN}🔗 GitHub Release URL: ${REPO_URL}/releases/new${NC}"
echo ""
echo -e "${YELLOW}Release Configuration:${NC}"
echo -e "   • Tag: v1.0.0"
echo -e "   • Title: Soft-UE ns-3 v1.0.0 - \"World's First Ultra Ethernet Protocol Stack\""
echo -e "   • Description: Use the content from IMMEDIATE_RELEASE_ACTIONS.md"
echo ""
echo -e "${CYAN}📝 Release Description Template:${NC}"
echo "   The release description is ready in 'IMMEDIATE_RELEASE_ACTIONS.md'"
echo "   Copy and paste the Release Notes section from that file."
echo ""

# Step 5: Community announcement
echo -e "${BLUE}📢 Step 5: Community Announcement${NC}"
echo ""
echo -e "${CYAN}📧 Announcement Templates:${NC}"
echo -e "   • Academic email: Use LAUNCH_ANNOUNCEMENT.md"
echo -e "   • Social media: Use the provided Twitter/X template"
echo -e "   • Technical forums: Use the community outreach kit"
echo ""

# Step 6: Verification
echo -e "${BLUE}🔍 Step 6: Post-Release Verification${NC}"
echo ""
echo -e "${CYAN}✅ Verification Checklist:${NC}"
echo -e "   [ ] GitHub repository created at: ${REPO_URL}"
echo -e "   [ ] Code pushed successfully"
echo -e "   [ ] v1.0.0 tag visible"
echo -e "   [ ] GitHub Release created"
echo -e "   [ ] README.md displays correctly"
echo -e "   [ ] LICENSE file present"
echo -e "   [ ] Community announcements sent"
echo ""

# Success message
echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  🎊 CONGRATULATIONS!                                             ║${NC}"
echo -e "${GREEN}║  You've just released the world's first Ultra Ethernet          ║${NC}"
echo -e "${GREEN}║  protocol stack open source implementation!                      ║${NC}"
echo -e "${GREEN}║                                                                  ║${NC}"
echo -e "${GREEN}║  This is a historic milestone in networking technology!       ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${PURPLE}🌟 Repository URL: ${REPO_URL}${NC}"
echo -e "${PURPLE}🚀 Release URL:   ${REPO_URL}/releases/tag/v1.0.0${NC}"
echo ""
echo -e "${CYAN}#UltraEthernet #OpenSource #TechHistory #Milestone${NC}"
echo ""

# Optional: Open the repository in browser (if on macOS)
if command -v open &> /dev/null; then
    echo -e "${BLUE}🌐 Opening repository in browser...${NC}"
    open "$REPO_URL"
fi

# Optional: Open the release creation page
if command -v open &> /dev/null; then
    echo -e "${BLUE}📝 Opening release creation page...${NC}"
    sleep 2
    open "${REPO_URL}/releases/new"
fi

echo -e "${GREEN}✨ Release automation completed successfully!${NC}"