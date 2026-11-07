/* See LICENSE file for license details. */
#define _XOPEN_SOURCE 500
#if HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/types.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xinerama.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>

#include "arg.h"
#include "util.h"

char *argv0;

/* global count to prevent repeated error messages */
int count_error = 0;

enum {
	BG,
	INIT,
	INPUT,
	FAILED,
	NUMCOLS
};

struct lock {
	int screen;
	Window root, win;
	Pixmap pmap;
	unsigned long colors[NUMCOLS];
	GC gc;
	XRRScreenResources *rrsr;
};

struct xrandr {
	int active;
	int evbase;
	int errbase;
};

#include "config.h"

static void
die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

#ifdef __linux__
#include <fcntl.h>
#include <linux/oom.h>

static void
dontkillme(void)
{
	FILE *f;
	const char oomfile[] = "/proc/self/oom_score_adj";

	if (!(f = fopen(oomfile, "w"))) {
		if (errno == ENOENT)
			return;
		die("slock: fopen %s: %s\n", oomfile, strerror(errno));
	}
	fprintf(f, "%d", OOM_SCORE_ADJ_MIN);
	if (fclose(f)) {
		if (errno == EACCES)
			die("slock: unable to disable OOM killer. "
			    "Make sure to suid or sgid slock.\n");
		else
			die("slock: fclose %s: %s\n", oomfile, strerror(errno));
	}
}
#endif

static int
linehasnonascii(const char *s, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if ((unsigned char)s[i] & 0x80)
			return 1;
	return 0;
}

static void
writemessage_box(Display *dpy, XftDraw *draw, XftColor *color,
                 XftFont *font_lat, XftFont *font_cjk,
                 int box_x, int box_y, int box_w, int box_h)
{
	int len, x, y, i, j, k, lines, lh, n;
	XftFont *font;
	XGlyphInfo ext;

	len = strlen(message);
	lines = 0;
	for (i = 0; i < len; i++)
		if (message[i] == '\n')
			lines++;

	lh = font_lat->height > font_cjk->height ? font_lat->height : font_cjk->height;
	/* 偏上：约在该显示器高度的 3/7 处，避免和中央方块重叠 */
	y = box_y + box_h * 3 / 7 - (lines * lh) / 2;

	for (i = j = k = 0; i <= len; i++) {
		if (i == len || message[i] == '\n') {
			n = i - j;
			font = linehasnonascii(message + j, n) ? font_cjk : font_lat;
			XftTextExtentsUtf8(dpy, font, (const FcChar8 *)(message + j), n, &ext);
			x = box_x + (box_w - ext.xOff) / 2;
			XftDrawStringUtf8(draw, color, font, x,
			                  y + font->ascent + k * lh,
			                  (const FcChar8 *)(message + j), n);
			while (i < len && message[i] == '\n') {
				i++;
				j = i;
				k++;
			}
		}
	}
}

/* 按 RandR CRTC（每块物理屏）分别居中；勿把双屏合成虚拟大屏再算一次中心 */
static void
writemessage(Display *dpy, struct lock *lock, struct xrandr *rr)
{
	int crtc, drew;
	XRRCrtcInfo *rrci;
	XWindowAttributes wa;
	XftFont *font_lat, *font_cjk;
	XftDraw *draw;
	XftColor color;
	Colormap cmap;
	Visual *vis;
	int screen = lock->screen;

	if (!message || !message[0])
		return;

	font_lat = XftFontOpenName(dpy, screen, font_name);
	font_cjk = font_name_cjk && font_name_cjk[0]
		? XftFontOpenName(dpy, screen, font_name_cjk) : NULL;
	if (!font_lat && !font_cjk) {
		if (count_error == 0) {
			fprintf(stderr, "slock: unable to load fonts \"%s\" / \"%s\"\n",
			        font_name, font_name_cjk ? font_name_cjk : "");
			count_error++;
		}
		return;
	}
	if (!font_lat)
		font_lat = font_cjk;
	if (!font_cjk)
		font_cjk = font_lat;

	vis = DefaultVisual(dpy, screen);
	cmap = DefaultColormap(dpy, screen);
	if (!XftColorAllocName(dpy, vis, cmap, text_color, &color)) {
		if (font_lat != font_cjk)
			XftFontClose(dpy, font_cjk);
		XftFontClose(dpy, font_lat);
		return;
	}

	draw = XftDrawCreate(dpy, lock->win, vis, cmap);
	drew = 0;
	if (rr && rr->active && lock->rrsr) {
		for (crtc = 0; crtc < lock->rrsr->ncrtc; crtc++) {
			rrci = XRRGetCrtcInfo(dpy, lock->rrsr, lock->rrsr->crtcs[crtc]);
			if (rrci->noutput > 0 && rrci->width > 0 && rrci->height > 0) {
				writemessage_box(dpy, draw, &color, font_lat, font_cjk,
				                 rrci->x, rrci->y, rrci->width, rrci->height);
				drew = 1;
			}
			XRRFreeCrtcInfo(rrci);
		}
	}
	if (!drew && XGetWindowAttributes(dpy, lock->win, &wa)) {
		writemessage_box(dpy, draw, &color, font_lat, font_cjk,
		                 0, 0, wa.width, wa.height);
	}

	XftDrawDestroy(draw);
	XftColorFree(dpy, vis, cmap, &color);
	if (font_lat != font_cjk)
		XftFontClose(dpy, font_cjk);
	XftFontClose(dpy, font_lat);
}



static const char *
gethash(void)
{
	const char *hash;
	struct passwd *pw;

	/* Check if the current user has a password entry */
	errno = 0;
	if (!(pw = getpwuid(getuid()))) {
		if (errno)
			die("slock: getpwuid: %s\n", strerror(errno));
		else
			die("slock: cannot retrieve password entry\n");
	}
	hash = pw->pw_passwd;

#if HAVE_SHADOW_H
	if (!strcmp(hash, "x")) {
		struct spwd *sp;
		if (!(sp = getspnam(pw->pw_name)))
			die("slock: getspnam: cannot retrieve shadow entry. "
			    "Make sure to suid or sgid slock.\n");
		hash = sp->sp_pwdp;
	}
#else
	if (!strcmp(hash, "*")) {
#ifdef __OpenBSD__
		if (!(pw = getpwuid_shadow(getuid())))
			die("slock: getpwnam_shadow: cannot retrieve shadow entry. "
			    "Make sure to suid or sgid slock.\n");
		hash = pw->pw_passwd;
#else
		die("slock: getpwuid: cannot retrieve shadow entry. "
		    "Make sure to suid or sgid slock.\n");
#endif /* __OpenBSD__ */
	}
#endif /* HAVE_SHADOW_H */

	return hash;
}

static void
draw(Display *dpy, struct xrandr *rr, struct lock **locks, int nscreens,
     unsigned int len, unsigned int color)
{
	int screen, crtc;
	XRRCrtcInfo* rrci;

	if (rr->active) {
		for (screen = 0; screen < nscreens; screen++) {
			XSetWindowBackground(dpy, locks[screen]->win,locks[screen]->colors[BG]);
			XClearWindow(dpy, locks[screen]->win);
			XSetForeground(dpy, locks[screen]->gc, locks[screen]->colors[color]);
			for (crtc = 0; crtc < locks[screen]->rrsr->ncrtc; ++crtc) {
				rrci = XRRGetCrtcInfo(dpy,
				                      locks[screen]->rrsr,
				                      locks[screen]->rrsr->crtcs[crtc]);
				/* skip disabled crtc */
				if (rrci->noutput > 0) {
					if (len < 1)
						len = 1;
					/* 方块略偏下，给上方 message（约 3/7 高）留空，横屏不易叠字 */
					int xoffset = (int) ((squaresize * len) + squarespacing * (len - 1)) / 2;
					int sqy = rrci->y + (rrci->height * 5 / 8) - squaresize / 2;
					for (int i = 0; i < len; i++)
						XFillRectangle(dpy,
						               locks[screen]->win,
						               locks[screen]->gc,
						               rrci->x - xoffset + ((rrci->width - squaresize) / 2 ) + ((squaresize + squarespacing) * i),
						               sqy,
						               squaresize,
						               squaresize);
				}
				XRRFreeCrtcInfo(rrci);
			}
			writemessage(dpy, locks[screen], rr);
		}
	} else {
		for (screen = 0; screen < nscreens; screen++) {
			XSetWindowBackground(dpy,
			                     locks[screen]->win,
			                     locks[screen]->colors[color]);
			XClearWindow(dpy, locks[screen]->win);
			writemessage(dpy, locks[screen], rr);
		}
	}
}

static void
readpw(Display *dpy, struct xrandr *rr, struct lock **locks, int nscreens,
       const char *hash)
{
	XRRScreenChangeNotifyEvent *rre;
	char buf[32], passwd[256], *inputhash;
	int num, screen, running, failure;
	unsigned int len, color;
	KeySym ksym;
	XEvent ev;

	len = 0;
	running = 1;
	failure = 0;

	while (running && !XNextEvent(dpy, &ev)) {
		if (ev.type == KeyPress) {
			explicit_bzero(&buf, sizeof(buf));
			num = XLookupString(&ev.xkey, buf, sizeof(buf), &ksym, 0);
			if (IsKeypadKey(ksym)) {
				if (ksym == XK_KP_Enter)
					ksym = XK_Return;
				else if (ksym >= XK_KP_0 && ksym <= XK_KP_9)
					ksym = (ksym - XK_KP_0) + XK_0;
			}
			if (IsFunctionKey(ksym) ||
			    IsKeypadKey(ksym) ||
			    IsMiscFunctionKey(ksym) ||
			    IsPFKey(ksym) ||
			    IsPrivateKeypadKey(ksym))
				continue;
			switch (ksym) {
			case XK_Return:
				passwd[len] = '\0';
				errno = 0;
				if (!(inputhash = crypt(passwd, hash)))
					fprintf(stderr, "slock: crypt: %s\n", strerror(errno));
				else
					running = !!strcmp(inputhash, hash);
				if (running) {
					XBell(dpy, 100);
					failure = 1;
				}
				explicit_bzero(&passwd, sizeof(passwd));
				len = 0;
				break;
			case XK_Escape:
				explicit_bzero(&passwd, sizeof(passwd));
				len = 0;
				break;
			case XK_BackSpace:
				if (len)
					passwd[--len] = '\0';
				break;
			default:
				if (num && !iscntrl((unsigned char)buf[0]) &&
				    (len + num < sizeof(passwd))) {
					memcpy(passwd + len, buf, num);
					len += num;
				} else if (buf[0] == '\025') { /* ctrl-u clears input */
					explicit_bzero(&passwd, sizeof(passwd));
					len = 0;
				}
				break;
			}
			color = len ? INPUT : ((failure || failonclear) ? FAILED : INIT);
			if (running) {
				draw(dpy, rr, locks, nscreens, len, color);
			}
		} else if (rr->active && ev.type == rr->evbase + RRScreenChangeNotify) {
			rre = (XRRScreenChangeNotifyEvent*)&ev;
			for (screen = 0; screen < nscreens; screen++) {
				if (locks[screen]->win == rre->window) {
					if (rre->rotation == RR_Rotate_90 ||
					    rre->rotation == RR_Rotate_270)
						XResizeWindow(dpy, locks[screen]->win,
						              rre->height, rre->width);
					else
						XResizeWindow(dpy, locks[screen]->win,
						              rre->width, rre->height);
					XClearWindow(dpy, locks[screen]->win);
					break;
				}
			}
		} else {
			for (screen = 0; screen < nscreens; screen++)
				XRaiseWindow(dpy, locks[screen]->win);
		}
	}
}

static struct lock *
lockscreen(Display *dpy, struct xrandr *rr, int screen)
{
	char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
	int i, ptgrab, kbgrab;
	struct lock *lock;
	XColor color, dummy;
	XSetWindowAttributes wa;
	Cursor invisible;
	XGCValues gcvalues;

	if (dpy == NULL || screen < 0 || !(lock = malloc(sizeof(struct lock))))
		return NULL;

	lock->screen = screen;
	lock->root = RootWindow(dpy, lock->screen);

	for (i = 0; i < NUMCOLS; i++) {
		XAllocNamedColor(dpy, DefaultColormap(dpy, lock->screen),
		                 colorname[i], &color, &dummy);
		lock->colors[i] = color.pixel;
	}

	/* init */
	wa.override_redirect = 1;
	wa.background_pixel = lock->colors[BG];
	lock->win = XCreateWindow(dpy, lock->root, 0, 0,
	                          DisplayWidth(dpy, lock->screen),
	                          DisplayHeight(dpy, lock->screen),
	                          0, DefaultDepth(dpy, lock->screen),
	                          CopyFromParent,
	                          DefaultVisual(dpy, lock->screen),
	                          CWOverrideRedirect | CWBackPixel, &wa);
	lock->pmap = XCreateBitmapFromData(dpy, lock->win, curs, 8, 8);
	invisible = XCreatePixmapCursor(dpy, lock->pmap, lock->pmap,
	                                &color, &color, 0, 0);
	XDefineCursor(dpy, lock->win, invisible);
	lock->gc = XCreateGC(dpy, lock->win, 0, &gcvalues);
	XSetForeground(dpy, lock->gc, lock->colors[INIT]);
	if (rr->active)
		lock->rrsr = XRRGetScreenResourcesCurrent(dpy, lock->root);

	/* Try to grab mouse pointer *and* keyboard for 600ms, else fail the lock */
	for (i = 0, ptgrab = kbgrab = -1; i < 6; i++) {
		if (ptgrab != GrabSuccess) {
			ptgrab = XGrabPointer(dpy, lock->root, False,
			                      ButtonPressMask | ButtonReleaseMask |
			                      PointerMotionMask, GrabModeAsync,
			                      GrabModeAsync, None, invisible, CurrentTime);
		}
		if (kbgrab != GrabSuccess) {
			kbgrab = XGrabKeyboard(dpy, lock->root, True,
			                       GrabModeAsync, GrabModeAsync, CurrentTime);
		}

		/* input is grabbed: we can lock the screen */
		if (ptgrab == GrabSuccess && kbgrab == GrabSuccess) {
			XMapRaised(dpy, lock->win);
			if (rr->active)
				XRRSelectInput(dpy, lock->win, RRScreenChangeNotifyMask);

			XSelectInput(dpy, lock->root, SubstructureNotifyMask);
			return lock;
		}

		/* retry on AlreadyGrabbed but fail on other errors */
		if ((ptgrab != AlreadyGrabbed && ptgrab != GrabSuccess) ||
		    (kbgrab != AlreadyGrabbed && kbgrab != GrabSuccess))
			break;

		usleep(100000);
	}

	/* we couldn't grab all input: fail out */
	if (ptgrab != GrabSuccess)
		fprintf(stderr, "slock: unable to grab mouse pointer for screen %d\n",
		        screen);
	if (kbgrab != GrabSuccess)
		fprintf(stderr, "slock: unable to grab keyboard for screen %d\n",
		        screen);
	return NULL;
}

static void
usage(void)
{
	die("usage: slock [-v] [-f] [-m message] [cmd [arg ...]]\n");
}

int
main(int argc, char **argv) {
	struct xrandr rr;
	struct lock **locks;
	struct passwd *pwd;
	struct group *grp;
	uid_t duid;
	gid_t dgid;
	const char *hash;
	Display *dpy;
	int s, nlocks, nscreens;
	char locktimebuf[64];
	time_t now;
	struct tm *tm;

	ARGBEGIN {
	case 'v':
		puts("slock-"VERSION);
		return 0;
	case 'm':
		message = EARGF(usage());
		break;
	case 'f':
		die("slock: message 使用 Xft；请用 fc-list 查看字体（如 Maple Mono NL NF）\n");
		return 0;
	default:
		usage();
	} ARGEND

	/* 未指定 -m 时显示锁定时刻 */
	if (!message || !message[0]) {
		now = time(NULL);
		tm = localtime(&now);
		if (!tm || !strftime(locktimebuf, sizeof(locktimebuf), locktimefmt, tm))
			snprintf(locktimebuf, sizeof(locktimebuf), "locked");
		message = locktimebuf;
	}

	/* validate drop-user and -group */
	errno = 0;
	if (!(pwd = getpwnam(user)))
		die("slock: getpwnam %s: %s\n", user,
		    errno ? strerror(errno) : "user entry not found");
	duid = pwd->pw_uid;
	errno = 0;
	if (!(grp = getgrnam(group)))
		die("slock: getgrnam %s: %s\n", group,
		    errno ? strerror(errno) : "group entry not found");
	dgid = grp->gr_gid;

#ifdef __linux__
	dontkillme();
#endif

	hash = gethash();
	errno = 0;
	if (!crypt("", hash))
		die("slock: crypt: %s\n", strerror(errno));

	if (!(dpy = XOpenDisplay(NULL)))
		die("slock: cannot open display\n");

	/* drop privileges */
	if (setgroups(0, NULL) < 0)
		die("slock: setgroups: %s\n", strerror(errno));
	if (setgid(dgid) < 0)
		die("slock: setgid: %s\n", strerror(errno));
	if (setuid(duid) < 0)
		die("slock: setuid: %s\n", strerror(errno));

	/* check for Xrandr support */
	rr.active = XRRQueryExtension(dpy, &rr.evbase, &rr.errbase);

	/* get number of screens in display "dpy" and blank them */
	nscreens = ScreenCount(dpy);
	if (!(locks = calloc(nscreens, sizeof(struct lock *))))
		die("slock: out of memory\n");
	for (nlocks = 0, s = 0; s < nscreens; s++) {
		if ((locks[s] = lockscreen(dpy, &rr, s)) != NULL)
			nlocks++;
		else
			break;
	}
	XSync(dpy, 0);

	/* did we manage to lock everything? */
	if (nlocks != nscreens)
		return 1;

	/* run post-lock command */
	if (argc > 0) {
		pid_t pid;
		extern char **environ;
		int err = posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ);
		if (err) {
			die("slock: failed to execute post-lock command: %s: %s\n",
			    argv[0], strerror(err));
		}
	}

	/* draw the initial rectangle */
	draw(dpy, &rr, locks, nscreens, 0, INIT);

	/* everything is now blank. Wait for the correct password */
	readpw(dpy, &rr, locks, nscreens, hash);

	return 0;
}
