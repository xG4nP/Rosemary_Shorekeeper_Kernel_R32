#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/property.h>
#include <linux/sched/signal.h>

#define MODULE_TAG "avi"
#define PROP_NAME "vendor.audio.voice.receiver.status"

static struct task_struct *interceptor_task;
static bool running = true;

static int avi_thread(void *data)
{
    char last_status[32] = {0};
    char current_status[32] = {0};
    struct device_node *node;
    const char *prop_value;
    int ret;
    
    pr_info(MODULE_TAG ": avi thread started\n");
    
    set_current_oom_score_adj(-1000);
    
    node = of_find_node_by_path("/");
    if (!node) {
        pr_err(MODULE_TAG ": Failed to find device node\n");
        return -ENODEV;
    }
    
    while (running && !kthread_should_stop()) {
      
        ret = of_property_read_string(node, PROP_NAME, &prop_value);
        if (ret) {

            ret = device_property_read_string(&node->dev, PROP_NAME, &prop_value);
            if (ret) {
                msleep(100);
                continue;
            }
        }
        
        strncpy(current_status, prop_value, sizeof(current_status) - 1);
        
        if (strcmp(current_status, "off") == 0 && 
            strcmp(last_status, "on") == 0) {
            
            pr_info(MODULE_TAG ": Detected on→off transition, killing audioserver\n");
            
            struct task_struct *p;
            rcu_read_lock();
            for_each_process(p) {
                if (strstr(p->comm, "audioserver") || 
                    strstr(p->comm, "audio")) {
                    pr_info(MODULE_TAG ": Killing PID %d (%s)\n", 
                           task_pid_nr(p), p->comm);
                    send_sig(SIGKILL, p, 0);
                }
            }
            rcu_read_unlock();
            
            msleep(3000);
            
            pr_info(MODULE_TAG ": avi active\n");
        }
        
        strncpy(last_status, current_status, sizeof(last_status) - 1);
        
        msleep(100);
    }
    
    pr_info(MODULE_TAG ": avi thread stopped\n");
    return 0;
}

static int __init avi_init(void)
{
    pr_info(MODULE_TAG ": Initializing avi\n");

    interceptor_task = kthread_run(avi_thread, NULL, 
                                   "avi");
    if (IS_ERR(interceptor_task)) {
        pr_err(MODULE_TAG ": Failed to create thread\n");
        return PTR_ERR(interceptor_task);
    }
    
    pr_info(MODULE_TAG ": avi started\n");
    return 0;
}

static void __exit avi_exit(void)
{
    pr_info(MODULE_TAG ": Stopping avi\n");
    
    running = false;
    if (interceptor_task) {
        kthread_stop(interceptor_task);
    }
    
    pr_info(MODULE_TAG ": avi removed\n");
}

module_init(avi_init);
module_exit(avi_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Patrick Ricafrente<xG4NPR>");
MODULE_DESCRIPTION("for audio problem");
MODULE_VERSION("1.0");
