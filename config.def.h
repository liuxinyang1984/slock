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

/* Xft：拉丁用 Maple；含汉字的行用 CJK（Maple 无汉字字形） */
static const char *font_name = "Maple Mono NL NF:size=32";
static const char *font_name_cjk = "Noto Sans CJK SC:size=32";
