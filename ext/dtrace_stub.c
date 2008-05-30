/* Ruby-Dtrace
 * (c) 2008 Chris Andrews <chris@nodnol.org>
 */

#include "dtrace_api.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>

RUBY_EXTERN VALUE eDtraceException;

#define FUNC_SIZE 64 /* good for 16 arguments: 16 + 3 * argc */

/* :nodoc: */
VALUE dtracestub_init(VALUE self, VALUE rargc)
{
  dtrace_stub_t *stub;
  char insns[FUNC_SIZE];
  char *ip = insns;
  int i;
  int argc = FIX2INT(rargc);

  Data_Get_Struct(self, dtrace_stub_t, stub);

#define OP_PUSHL_EBP     0x55
#define OP_MOVL_ESP_EBP  0x89, 0xe5
#define OP_SUBL_N_ESP    0x83, 0xec
#define OP_PUSHL_N_EBP_U 0xff
#define OP_PUSHL_N_EBP_L 0x75
#define OP_NOP           0x90
#define OP_ADDL_ESP_U    0x83
#define OP_ADDL_ESP_L    0xc4
#define OP_LEAVE         0xc9
#define OP_RET           0xc3

  char func_in[7] = {
    OP_PUSHL_EBP, OP_MOVL_ESP_EBP, OP_SUBL_N_ESP, 0x08, NULL
  };

  char func_out[3] = {
    OP_LEAVE, OP_RET, NULL
  };

  for (i = 0; func_in[i]; i++)
    *ip++ = func_in[i];

  for (i = (4 + 4*argc); i >= 0x08; i -= 4) {
    *ip++ = OP_PUSHL_N_EBP_U;
    *ip++ = OP_PUSHL_N_EBP_L;
    *ip++ = i;
  }

  for (i = 0; i <=5; i++)
    *ip++ = OP_NOP;

  *ip++ = OP_ADDL_ESP_U;
  *ip++ = OP_ADDL_ESP_L;
  *ip++ = argc * 4;
  
  for (i = 0; func_out[i]; i++)
    *ip++ = func_out[i];

  stub->mem  = NULL;
  stub->func = NULL;

  /* create stub function, get offset */ 

  /* allocate memory on a page boundary, for mprotect: valloc on OSX,
     memalign(PAGESIZE, ...) on Solaris. */
#ifdef __APPLE__
  if ((stub->mem = (void *)valloc(FUNC_SIZE)) < 0) {
#else
  if ((stub->mem = (void *)memalign(PAGESIZE, FUNC_SIZE)) < 0) {
#endif
    rb_raise(eDtraceException, "malloc failed: %s\n", strerror(errno));
    return Qnil;
  }

  if ((mprotect(stub->mem, FUNC_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC)) < 0) {
    rb_raise(eDtraceException, "mprotect failed: %s\n", strerror(errno));
    return Qnil;
  }
  
  if ((memcpy(stub->mem, insns, FUNC_SIZE)) < 0) {
    rb_raise(eDtraceException, "memcpy failed: %s\n", strerror(errno));
    return Qnil;
  }    
  
  stub->func = stub->mem;

  return self;
}

VALUE dtracestub_alloc(VALUE klass)
{
  VALUE obj;
  dtrace_stub_t *stub;

  stub = ALLOC(dtrace_stub_t);
  if (!stub) {
    rb_raise(eDtraceException, "alloc failed");
    return Qnil;
  }

  /* obj = Data_Wrap_Struct(klass, dtrace_hdl_mark, dtrace_hdl_free, handle); */
  obj = Data_Wrap_Struct(klass, NULL, NULL, stub);
  return obj;
}

VALUE dtracestub_addr(VALUE self) {
  dtrace_stub_t *stub;
  int ret;
  
  Data_Get_Struct(self, dtrace_stub_t, stub);
  return INT2FIX(stub->mem);
}
   
VALUE dtracestub_call(int argc, VALUE *ruby_argv, VALUE self) {
  dtrace_stub_t *stub;
  int i;
  void *argv[8]; // probe argc max for now.

  Data_Get_Struct(self, dtrace_stub_t, stub);

  /* munge Ruby values to either char *s or ints. */
  for (i = 0; i < argc; i++) {
    switch (TYPE(ruby_argv[i])) {
    case T_STRING:
      argv[i] = (void *)RSTRING(ruby_argv[i])->ptr;
      break;
    case T_FIXNUM:
      argv[i] = (void *)FIX2INT(ruby_argv[i]);
      break;
    default:
      rb_raise(eDtraceException, "type of arg[%d] is not string or fixnum", i);
      break;
    }
  }

  switch (argc) {
  case 0:
    (void)(*stub->func)();
    break;
  case 1:
    (void)(*stub->func)(argv[0]);
    break;
  case 2:
    (void)(*stub->func)(argv[0], argv[1]);
    break;
  case 3:
    (void)(*stub->func)(argv[0], argv[1], argv[2]);
    break;
  case 4:
    (void)(*stub->func)(argv[0], argv[1], argv[2], argv[3]);
    break;
  case 5:
    (void)(*stub->func)(argv[0], argv[1], argv[2], argv[3], 
                        argv[4]);
    break;
  case 6:
    (void)(*stub->func)(argv[0], argv[1], argv[2], argv[3],
                        argv[4], argv[5]);
    break;
  case 7:
    (void)(*stub->func)(argv[0], argv[1], argv[2], argv[3],
                        argv[4], argv[5], argv[6]);
    break;
  case 8:
    (void)(*stub->func)(argv[0], argv[1], argv[2], argv[3],
                        argv[4], argv[5], argv[6], argv[7]);
    break;
  default:
    rb_raise(eDtraceException, "probe argc max is 8");
    break;
  }
  
  return Qnil;
}
