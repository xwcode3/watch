#!/bin/zsh

echo "开始推送 master 分支到所有的远程仓库"

echo "\n 推送到 github_repository master"
git push github_repository "master"

echo "\n 推送到 gitee_repository master"
git push gitee_repository "master"

echo "\n 所有推送完成"
