#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/string.h>

static BLOCKING_NOTIFIER_HEAD(voice_status_notifier_list);

static int voice_prop_notifier(struct notifier_block *nb,
                               unsigned long action, void *data)
{
    const char *prop_name = data;
    const char *prop_value;
    
    if (strcmp(prop_name, "vendor.audio.voice.receiver.status") != 0)
        return NOTIFY_DONE;
    
    prop_value = "off"; // You'd read from actual property
    
    if (strcmp(prop_value, "off") == 0) {
        pr_info("avi: Killing audioserver\n");

      
        blocking_notifier_call_chain(&voice_status_notifier_list, 
                                    0, "kill_audioserver");
        
        msleep(3000);
    }
    
    return NOTIFY_OK;
}

static struct notifier_block voice_nb = {
    .notifier_call = voice_prop_notifier,
};

static int audio_server_killer(struct notifier_block *nb,
                              unsigned long action, void *data)
{
    if (strcmp(data, "kill_audioserver") == 0) {
        struct task_struct *p;
        
        rcu_read_lock();
        for_each_process(p) {
            if (strstr(p->comm, "audioserver")) {
                force_sig(SIGKILL, p);
                pr_info("Killed audioserver PID %d\n", task_pid_nr(p));
            }
        }
        rcu_read_unlock();
    }
    return NOTIFY_OK;
}

static struct notifier_block audio_kill_nb = {
    .notifier_call = audio_server_killer,
};

static int __init mt6785_avi_init(void)
{
    int ret;
    
    ret = register_property_notifier(&voice_nb);
    if (ret) {
        pr_err("Failed to register property notifier\n");
        return ret;
    }
    
    blocking_notifier_chain_register(&voice_status_notifier_list, &audio_kill_nb);
    
    pr_info("MT6785 avi initialized\n");
    return 0;
}

static void __exit mt6785_avi_exit(void)
{
    unregister_property_notifier(&voice_nb);
    blocking_notifier_chain_unregister(&voice_status_notifier_list, &audio_kill_nb);
    pr_info("MT6785 avi removed\n");
}
