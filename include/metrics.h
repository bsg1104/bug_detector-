#ifndef METRICS_H
#define METRICS_H

void metrics_inc_allowed();
void metrics_inc_rejected(const char *reason);
char *metrics_get(); /* returns heap string, free after use */

#endif
