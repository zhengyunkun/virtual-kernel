#ifndef _VKERNEL_CUSTOM_H
#define _VKERNEL_CUSTOM_H
#include <linux/key.h>
#include <linux/kexec.h>
#include <linux/utsname.h>
#include <linux/syscalls.h>
asmlinkage long (*orig_sys_clone)(unsigned long arg1, unsigned long arg2, int __user * arg3, int __user * arg4, unsigned long arg5);
asmlinkage long (*orig_sys_personality)(unsigned int personality);
asmlinkage long (*orig_sys_pkey_free)(int pkey);
asmlinkage long (*orig_sys_fsopen)(const char __user *fs_name, unsigned int flags);
asmlinkage long (*orig_sys_add_key)(const char __user *_type, const char __user *_description, const void __user *_payload, size_t plen, key_serial_t destringid);
asmlinkage long (*orig_sys_swapon)(const char __user *specialfile, int swap_flags);
asmlinkage long (*orig_sys_ustat)(unsigned dev, struct ustat __user *ubuf);
asmlinkage long (*orig_sys_kexec_file_load)(int kernel_fd, int initrd_fd, unsigned long cmdline_len, const char __user *cmdline_ptr, unsigned long flags);
asmlinkage long (*orig_sys_move_mount)(int from_dfd, const char __user *from_path, int to_dfd, const char __user *to_path, unsigned int ms_flags);
asmlinkage long (*orig_sys_kexec_load)(unsigned long entry, unsigned long nr_segments, struct kexec_segment __user *segments, unsigned long flags);
asmlinkage long (*orig_sys_fsmount)(int fs_fd, unsigned int flags, unsigned int ms_flags);
asmlinkage long (*orig_sys_pkey_alloc)(unsigned long flags, unsigned long init_val);
asmlinkage long (*orig_sys_keyctl)(int cmd, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);
asmlinkage long (*orig_sys_swapoff)(const char __user *specialfile);
asmlinkage long (*orig_sys_pkey_mprotect)(unsigned long start, size_t len, unsigned long prot, int pkey);
asmlinkage long (*orig_sys_sysfs)(int option, unsigned long arg1, unsigned long arg2);
asmlinkage long (*orig_sys_ni_syscall)(void);
asmlinkage long (*orig_sys_request_key)(const char __user *_type, const char __user *_description, const char __user *_callout_info, key_serial_t destringid);
asmlinkage long (*orig_sys_migrate_pages)(pid_t pid, unsigned long maxnode, const unsigned long __user *from, const unsigned long __user *to);
asmlinkage long (*orig_sys_open_tree)(int dfd, const char __user *path, unsigned flags);
asmlinkage long (*orig_sys_fspick)(int dfd, const char __user *path, unsigned int flags);
asmlinkage long (*orig_sys_userfaultfd)(int flags);
asmlinkage long (*orig_sys_pivot_root)(const char __user *new_root, const char __user *put_old);
asmlinkage long (*orig_sys_fsconfig)(int fs_fd, unsigned int cmd, const char __user *key, const void __user *value, int aux);
asmlinkage long (*orig_sys_clone3)(struct clone_args __user *uargs, size_t size);
asmlinkage long (*orig_sys_move_pages)(pid_t pid, unsigned long nr_pages, const void __user * __user *pages, const int __user *nodes, int __user *status, int flags);

asmlinkage long vkn_sys_clone(unsigned long arg1, unsigned long arg2, int __user * arg3, int __user * arg4, unsigned long arg5);
asmlinkage long vkn_sys_personality(unsigned int personality);
asmlinkage long vkn_sys_pkey_free(int pkey);
asmlinkage long vkn_sys_fsopen(const char __user *fs_name, unsigned int flags);
asmlinkage long vkn_sys_add_key(const char __user *_type, const char __user *_description, const void __user *_payload, size_t plen, key_serial_t destringid);
asmlinkage long vkn_sys_swapon(const char __user *specialfile, int swap_flags);
asmlinkage long vkn_sys_ustat(unsigned dev, struct ustat __user *ubuf);
asmlinkage long vkn_sys_kexec_file_load(int kernel_fd, int initrd_fd, unsigned long cmdline_len, const char __user *cmdline_ptr, unsigned long flags);
asmlinkage long vkn_sys_move_mount(int from_dfd, const char __user *from_path, int to_dfd, const char __user *to_path, unsigned int ms_flags);
asmlinkage long vkn_sys_kexec_load(unsigned long entry, unsigned long nr_segments, struct kexec_segment __user *segments, unsigned long flags);
asmlinkage long vkn_sys_fsmount(int fs_fd, unsigned int flags, unsigned int ms_flags);
asmlinkage long vkn_sys_pkey_alloc(unsigned long flags, unsigned long init_val);
asmlinkage long vkn_sys_keyctl(int cmd, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);
asmlinkage long vkn_sys_swapoff(const char __user *specialfile);
asmlinkage long vkn_sys_pkey_mprotect(unsigned long start, size_t len, unsigned long prot, int pkey);
asmlinkage long vkn_sys_sysfs(int option, unsigned long arg1, unsigned long arg2);
asmlinkage long vkn_sys_ni_syscall(void);
asmlinkage long vkn_sys_request_key(const char __user *_type, const char __user *_description, const char __user *_callout_info, key_serial_t destringid);
asmlinkage long vkn_sys_migrate_pages(pid_t pid, unsigned long maxnode, const unsigned long __user *from, const unsigned long __user *to);
asmlinkage long vkn_sys_open_tree(int dfd, const char __user *path, unsigned flags);
asmlinkage long vkn_sys_fspick(int dfd, const char __user *path, unsigned int flags);
asmlinkage long vkn_sys_userfaultfd(int flags);
asmlinkage long vkn_sys_pivot_root(const char __user *new_root, const char __user *put_old);
asmlinkage long vkn_sys_fsconfig(int fs_fd, unsigned int cmd, const char __user *key, const void __user *value, int aux);
asmlinkage long vkn_sys_clone3(struct clone_args __user *uargs, size_t size);
asmlinkage long vkn_sys_move_pages(pid_t pid, unsigned long nr_pages, const void __user * __user *pages, const int __user *nodes, int __user *status, int flags);


asmlinkage long vkn_sys_clone(unsigned long arg1, unsigned long arg2, int __user * arg3, int __user * arg4, unsigned long arg5){
        struct pt_regs *regs;
        regs = task_pt_regs(current);
        
        if( ( (2114060288 & regs->di) == 0) ){
            return orig_sys_clone(arg1, arg2, arg3, arg4, arg5);
        }
    
        return -1;
}

asmlinkage long vkn_sys_personality(unsigned int personality){
        struct pt_regs *regs;
        regs = task_pt_regs(current);
        
        if( (regs->di == 0) ){
            return orig_sys_personality(personality);
        }
    
        if( (regs->di == 8) ){
            return orig_sys_personality(personality);
        }
    
        if( (regs->di == 131072) ){
            return orig_sys_personality(personality);
        }
    
        if( (regs->di == 131080) ){
            return orig_sys_personality(personality);
        }
    
        if( (regs->di == 4294967295) ){
            return orig_sys_personality(personality);
        }
    
        return -1;
}

asmlinkage long vkn_sys_pkey_free(int pkey){
    return -1;
}

asmlinkage long vkn_sys_fsopen(const char __user *fs_name, unsigned int flags){
    return -1;
}

asmlinkage long vkn_sys_add_key(const char __user *_type, const char __user *_description, const void __user *_payload, size_t plen, key_serial_t destringid){
    return -1;
}

asmlinkage long vkn_sys_swapon(const char __user *specialfile, int swap_flags){
    return -1;
}

asmlinkage long vkn_sys_ustat(unsigned dev, struct ustat __user *ubuf){
    return -1;
}

asmlinkage long vkn_sys_kexec_file_load(int kernel_fd, int initrd_fd, unsigned long cmdline_len, const char __user *cmdline_ptr, unsigned long flags){
    return -1;
}

asmlinkage long vkn_sys_move_mount(int from_dfd, const char __user *from_path, int to_dfd, const char __user *to_path, unsigned int ms_flags){
    return -1;
}

asmlinkage long vkn_sys_kexec_load(unsigned long entry, unsigned long nr_segments, struct kexec_segment __user *segments, unsigned long flags){
    return -1;
}

asmlinkage long vkn_sys_fsmount(int fs_fd, unsigned int flags, unsigned int ms_flags){
    return -1;
}

asmlinkage long vkn_sys_pkey_alloc(unsigned long flags, unsigned long init_val){
    return -1;
}

asmlinkage long vkn_sys_keyctl(int cmd, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5){
    return -1;
}

asmlinkage long vkn_sys_swapoff(const char __user *specialfile){
    return -1;
}

asmlinkage long vkn_sys_pkey_mprotect(unsigned long start, size_t len, unsigned long prot, int pkey){
    return -1;
}

asmlinkage long vkn_sys_sysfs(int option, unsigned long arg1, unsigned long arg2){
    return -1;
}

asmlinkage long vkn_sys_ni_syscall(void){
    return -1;
}

asmlinkage long vkn_sys_request_key(const char __user *_type, const char __user *_description, const char __user *_callout_info, key_serial_t destringid){
    return -1;
}

asmlinkage long vkn_sys_migrate_pages(pid_t pid, unsigned long maxnode, const unsigned long __user *from, const unsigned long __user *to){
    return -1;
}

asmlinkage long vkn_sys_open_tree(int dfd, const char __user *path, unsigned flags){
    return -1;
}

asmlinkage long vkn_sys_fspick(int dfd, const char __user *path, unsigned int flags){
    return -1;
}

asmlinkage long vkn_sys_userfaultfd(int flags){
    return -1;
}

asmlinkage long vkn_sys_pivot_root(const char __user *new_root, const char __user *put_old){
    return -1;
}

asmlinkage long vkn_sys_fsconfig(int fs_fd, unsigned int cmd, const char __user *key, const void __user *value, int aux){
    return -1;
}

asmlinkage long vkn_sys_clone3(struct clone_args __user *uargs, size_t size){
    return -1;
}

asmlinkage long vkn_sys_move_pages(pid_t pid, unsigned long nr_pages, const void __user * __user *pages, const int __user *nodes, int __user *status, int flags){
    return -1;
}

    /**
     *   ### ToDo:
     *   0: deny this syscall.(default)
     *   1: allow this syscall.
     *   2: we made this syscall in our own way, like, futex.
     *   3: syscall with args check .(default : personality(135)and clone(56))
     *   6: non-exist
     *
     * */
#endif /* _VKERNEL_CUSTOM_H */
    