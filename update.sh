#!/bin/bash

# Linux-BP 上传脚本

echo "=== Linux-BP GitHub Upload Script ==="

# 配置
GITHUB_USER="Kaidevon"
REPO_NAME="Linux-BP"
REMOTE_URL="https://github.com/${GITHUB_USER}/${REPO_NAME}.git"

# 步骤 1：初始化 Git
if [ ! -d ".git" ]; then
    echo "[1/5] Initializing Git repository..."
    git init
fi

# 步骤 2：添加远程仓库
echo "[2/5] Adding remote repository..."
git remote remove origin 2>/dev/null || true
git remote add origin ${REMOTE_URL}

# 步骤 3：添加文件
echo "[3/5] Adding files..."
git add .

# 步骤 4：提交
echo "[4/5] Committing..."
git commit -m "Initial commit: Linux-BP with hw_bp and sw_bp modules" || echo "Nothing to commit"

# 步骤 5：推送
echo "[5/5] Pushing to GitHub..."
git branch -M main
git push -u origin main

echo "=== Done! Check your GitHub repository ==="
