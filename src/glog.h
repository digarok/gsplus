#ifdef __cplusplus
extern "C" {
#endif
  int glog(const char *s);
  int glogf(const char *s, ...);

  int fatal_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
