/* user and group to drop privileges to */
static const char *user  = "nobody";
static const char *group = "nobody"; /* Arch: nobody；上游默认 nogroup */

static const char *colorname[NUMCOLS] = {
	[BG] =     "black",     /* background */
	[INIT] =   "#4f525c",   /* after initialization */
	[INPUT] =  "#005577",   /* during input */
	[FAILED] = "#CC3333",   /* wrong password */
};

/* treat a cleared input like a wrong password (color) */
static const int failonclear = 1;

/* size of square in px */
static const int squaresize = 28;

/* spacing between squares in px */
static const int squarespacing = 8;

/*
 * 锁屏提示：NULL/空串 = 锁定时自动写成当前时间（strftime 格式见 locktimefmt）。
 * 也可用 -m 覆盖。
 */
static const char *message =
	"是谁在呼叫舰队？\n"
	"Who called in the fleet?";
static const char *locktimefmt = "%Y-%m-%d %H:%M";

/* text color */
static const char *text_color = "#eeeeee";

/*
 * Xft：slock 为 setuid，跑在 root/nobody 下，读不到 ~/.config/fontconfig。
 * 用户级的 sans→Noto CJK 链不生效，sans-serif 会落到无汉字的 Noto Sans。
 * CJK 必须写死系统路径里的族名（Noto Sans CJK SC 在 /usr/share/fonts）。
 */
static const char *font_name = "monospace:size=32";
static const char *font_name_cjk = "Noto Sans CJK SC:size=32";
