# 本机补丁

基于上游 slock **1.7**，叠：

1. [squares](https://tools.suckless.org/slock/patches/squares/)（`slock-squares-1.5.diff`）
2. [squares-password](https://tools.suckless.org/slock/patches/squares/)（输入长度 → 方块数）
3. [message](https://tools.suckless.org/slock/patches/message/)（提示文字；与 1.7/squares 手工合并）

额外改动：

- Arch：`group = "nobody"`
- 每次 `draw()` 清屏后重画 message（否则方块刷新会冲掉文字）
- message 改 **Xft**：拉丁 Maple、汉字行 Noto Sans CJK SC
- 默认文案：星际争霸「是谁在呼叫舰队？ / Who calls the fleet?」
- `NULL` message 仍可回退为锁定时间（`locktimefmt`）

原版 diff 在 `patches/`，仅作对照；工作树已是合并结果。

```bash
# 覆盖运行时提示
slock -m "away"
# 字体：fc-list | grep -i maple
```
