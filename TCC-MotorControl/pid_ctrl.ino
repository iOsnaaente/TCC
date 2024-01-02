#include <sys/param.h>
#include <stdbool.h>
#include <stdlib.h>

typedef enum {
    PID_CAL_TYPE_INCREMENTAL, /*!< Incremental PID control */
    PID_CAL_TYPE_POSITIONAL,  /*!< Positional PID control */
} pid_calculate_type_t;

typedef struct pid_ctrl_block_t *pid_ctrl_block_handle_t;

typedef struct {
    float kp;                      // PID Kp parameter
    float ki;                      // PID Ki parameter
    float kd;                      // PID Kd parameter
    float max_output;              // PID maximum output limitation
    float min_output;              // PID minimum output limitation
    float max_integral;            // PID maximum integral value limitation
    float min_integral;            // PID minimum integral value limitation
    pid_calculate_type_t cal_type; // PID calculation type
} pid_ctrl_parameter_t;

typedef struct {
    pid_ctrl_parameter_t init_param; // Initial parameters
} pid_ctrl_config_t;


typedef struct pid_ctrl_block_t pid_ctrl_block_t;
typedef float (*pid_cal_func_t)(pid_ctrl_block_t *pid, float error);

struct pid_ctrl_block_t {
    float Kp;                       // PID Kp value
    float Ki;                       // PID Ki value
    float Kd;                       // PID Kd value
    float previous_err1;            // e(k-1)
    float previous_err2;            // e(k-2)
    float integral_err;             // Sum of error
    float last_output;              // PID output in last control period
    float max_output;               // PID maximum output limitation
    float min_output;               // PID minimum output limitation
    float max_integral;             // PID maximum integral value limitation
    float min_integral;             // PID minimum integral value limitation
    pid_cal_func_t calculate_func;  // calculation function, depends on actual PID type set by user
};

static float pid_calc_positional(pid_ctrl_block_t *pid, float error){
    float output = 0;
    /* Add current error to the integral error */
    pid->integral_err += error;
    /* If the integral error is out of the range, it will be limited */
    pid->integral_err = MIN(pid->integral_err, pid->max_integral);
    pid->integral_err = MAX(pid->integral_err, pid->min_integral);

    /* Calculate the pid control value by location formula */
    /* u(k) = e(k)*Kp + (e(k)-e(k-1))*Kd + integral*Ki */
    output = error * pid->Kp +
             (error - pid->previous_err1) * pid->Kd +
             pid->integral_err * pid->Ki;

    /* If the output is out of the range, it will be limited */
    output = MIN(output, pid->max_output);
    output = MAX(output, pid->min_output);

    /* Update previous error */
    pid->previous_err1 = error;

    return output;
}

static float pid_calc_incremental(pid_ctrl_block_t *pid, float error){
    float output = 0;
    
    /* Calculate the pid control value by increment formula */
    /* du(k) = (e(k)-e(k-1))*Kp + (e(k)-2*e(k-1)+e(k-2))*Kd + e(k)*Ki */
    /* u(k) = du(k) + u(k-1) */
    output = (error - pid->previous_err1) * pid->Kp +
             (error - 2 * pid->previous_err1 + pid->previous_err2) * pid->Kd +
             error * pid->Ki +
             pid->last_output;

    /* If the output is beyond the range, it will be limited */
    output = MIN(output, pid->max_output);
    output = MAX(output, pid->min_output);

    /* Update previous error */
    pid->previous_err2 = pid->previous_err1;
    pid->previous_err1 = error;

    /* Update last output */
    pid->last_output = output;

    return output;
}

void pid_new_control_block( const pid_ctrl_config_t *config, pid_ctrl_block_handle_t *ret_pid){
    pid_ctrl_block_t *pid = NULL;
    /* Check the input pointer */
    pid = calloc(1, sizeof(pid_ctrl_block_t));
    pid_update_parameters(pid, &config->init_param ); // "init PID parameters failed"
    *ret_pid = pid;
}

void pid_del_control_block( pid_ctrl_block_handle_t pid){
    free(pid);
}

void pid_compute( pid_ctrl_block_handle_t pid, float input_error, float *ret_result){
    *ret_result = pid->calculate_func(pid, input_error);
}

void pid_update_parameters( pid_ctrl_block_handle_t pid, const pid_ctrl_parameter_t *params){
    pid->Kp = params->kp;
    pid->Ki = params->ki;
    pid->Kd = params->kd;
    pid->max_output = params->max_output;
    pid->min_output = params->min_output;
    pid->max_integral = params->max_integral;
    pid->min_integral = params->min_integral;

    /* Set the calculate function according to the PID type */
    switch (params->cal_type) {
    case PID_CAL_TYPE_INCREMENTAL:
        pid->calculate_func = pid_calc_incremental;
        break;
    case PID_CAL_TYPE_POSITIONAL:
        pid->calculate_func = pid_calc_positional;
        break;
    default:
        break;
    }
}
