#ifndef AI_INFERENCE_H
#define AI_INFERENCE_H

#include <stdbool.h>

/* Model IO sizes (fixed for current model) */
#define AI_IN_SIZE 6
#define AI_OUT_SIZE 3

bool AiInference_Init(void);
int  AiInference_RunFromArray(const float in[AI_IN_SIZE]);
void AiInference_Deinit(void);

#endif /* AI_INFERENCE_H */
