#ifndef TAU_SOS_H
#define TAU_SOS_H

#define TAU_SOS_INTERRUPT_PERIOD 2 // two seconds

#ifdef __cplusplus
extern "C" {  // export a C interface for C++ codes
#else
#include <stdbool.h> // import bool support for C codes
#endif
void TAU_SOS_send_data(void);
void TAU_SOS_init(void);
void TAU_SOS_stop_worker(void);
void TAU_SOS_finalize(void);
void TAU_SOS_send_data(void);
void Tau_SOS_pack_current_timer(const char * event_name);
void Tau_SOS_pack_string(const char * name, char * value);
void Tau_SOS_pack_double(const char * name, double value);
void Tau_SOS_pack_integer(const char * name, int value);
void Tau_SOS_pack_long(const char * name, long int value);
void * Tau_sos_thread_function(void* data);
#ifdef __cplusplus
}
#endif

#endif // TAU_SOS_H
