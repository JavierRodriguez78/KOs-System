#ifndef KOS_PS_SERVICE_H
#define KOS_PS_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

// Fills buffer with process info in text format. Returns number of bytes written.
int ps_service_getinfo(char* buffer, int maxlen);

#ifdef __cplusplus
}
#endif

#endif
