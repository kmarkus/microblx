#ifndef UBX_UTILS_H
#define UBX_UTILS_H

int ubx_wait_sigint(unsigned int timeout_s);
void char_replace(char *s, const char find, const char rep);

#endif /* UBX_UTILS_H */
