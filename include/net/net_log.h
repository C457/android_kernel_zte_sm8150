#ifndef _NET_LOG_H
#define _NET_LOG_H

#ifdef CONFIG_PRINTK_EXT
extern int printk_ext(const char *fmt, ...);

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif
#define pr_log_err(fmt, ...) \
	printk_ext(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define pr_log_warn(fmt, ...) \
	printk_ext(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define pr_log_info(fmt, ...) \
	printk_ext(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#define pr_log_debug(fmt, ...) \
	printk_ext(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else  /* CONFIG_PRINTK_EXT is not defined */
#define pr_log_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)
#define pr_log_warn(fmt, ...) \
	pr_warn(fmt, ##__VA_ARGS__)
#define pr_log_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)
#define pr_log_debug(fmt, ...) \
	pr_debug(fmt, ##__VA_ARGS__)
#endif

#endif  /* _NET_LOG_H */
