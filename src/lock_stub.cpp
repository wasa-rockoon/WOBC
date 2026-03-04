extern "C" {
  void __retarget_lock_init_recursive(void *lock) { }
  void __retarget_lock_close_recursive(void *lock) { }
  void __retarget_lock_acquire_recursive(void *lock) { }
  void __retarget_lock_release_recursive(void *lock) { }
}