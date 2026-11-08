#!/bin/sh
# 安装 slock（锁屏）+ 可选 xss-lock（空闲触发）
#   ./install.sh
#   ./install.sh --no-packages
#   ./install.sh --user
# 父仓: ./install.sh slock  或  desktop:suckless
#
# 空闲 5 分钟锁屏 / 15 分钟灭屏由 dwm-session 配置（xss-lock → slock），
# 不在本二进制内。需同时安装: ../../../../config/x11/dwm/install.sh
# 或父仓 desktop:suckless。
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
DO_PKGS=1
MODE=system

usage() {
	cat <<'EOF'
用法: ./install.sh [选项]

安装 suckless slock（message + squares-password），并默认装 xss-lock。

选项:
  （默认）         提权 make install → /usr/local/bin/slock（setuid）
  --user           make PREFIX=$HOME/.local install（仍建议系统装以便 setuid root）
  --no-packages    不 pacman 装 xss-lock / 依赖
  -h, --help       显示本说明

依赖（Arch，--no-packages 时跳过）:
  xss-lock         空闲时调 slock（dwm-session 需要）
  libxft fontconfig 等（编 slock message 补丁）

落地:
  $PREFIX/bin/slock          （默认 /usr/local；setuid）
  pacman: xss-lock

与空闲定时的关系（在 dwm-session，不在 slock 里）:
  5 分钟  → xset s + xss-lock → slock
  15 分钟 → xset dpms 灭屏
  覆盖: IDLE_LOCK_SEC / IDLE_DPMS_SEC
  会话脚本: 父仓 ./config/x11/dwm/install.sh 或 desktop:suckless

手动锁屏: dwm Super+Ctrl+l → slock
测: slock   # 输入密码解锁；Ctrl+Alt+F2 救急

示例:
  ./install.sh
  ./install.sh --no-packages
  ./install.sh --user --no-packages
EOF
}

while [ $# -gt 0 ]; do
	case "$1" in
	--user) MODE=user ;;
	--no-packages) DO_PKGS=0 ;;
	-h|--help) usage; exit 0 ;;
	-*)
		echo "未知选项: $1" >&2
		usage >&2
		exit 1
		;;
	*)
		echo "未知参数: $1" >&2
		usage >&2
		exit 1
		;;
	esac
	shift
done

run_root() {
	if [ "$(id -u)" -eq 0 ]; then
		"$@"
	elif command -v doas >/dev/null 2>&1; then
		doas "$@"
	elif command -v sudo >/dev/null 2>&1; then
		sudo "$@"
	else
		echo "需要 root（doas/sudo）" >&2
		exit 1
	fi
}

is_arch() {
	[ -f /etc/arch-release ] || grep -qi arch /etc/os-release 2>/dev/null
}

if [ "$DO_PKGS" = 1 ] && is_arch; then
	echo "pacman: xss-lock（空闲锁屏）+ 编译依赖"
	run_root pacman -S --needed --noconfirm xss-lock \
		libx11 libxext libxrandr libxinerama libxft fontconfig freetype2 \
		|| true
elif [ "$DO_PKGS" = 1 ]; then
	echo "非 Arch：请自行安装 xss-lock 与 X11/Xft 开发库" >&2
fi

if ! command -v xss-lock >/dev/null 2>&1; then
	echo "警告: PATH 中无 xss-lock；dwm-session 空闲锁屏不会启动（仅手动 slock）" >&2
fi

echo "==== slock（build） ===="
(
	cd "$ROOT"
	# 保留已有 config.h；无则 Makefile 从 config.def.h 生成
	make
	if [ "$MODE" = user ]; then
		echo "安装 → $HOME/.local（注意: setuid 非 root 时解锁能力可能不足）"
		make PREFIX="$HOME/.local" install
	else
		echo "安装 → config.mk PREFIX（需提权；setuid）"
		run_root make install
	fi
	make clean 2>/dev/null || true
)

echo "完成。二进制: $(command -v slock 2>/dev/null || echo '请检查 PATH')"
echo "  空闲 5m/15m: 确保已装 dwm-session（父仓 config/x11/dwm/install.sh）"
echo "  测: slock"
