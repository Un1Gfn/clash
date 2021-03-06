#include <assert.h>
#include <pwd.h>
#include <unistd.h>

#include "./privilege.h"
#include "./def.h"

void privilege_escalate(){
  assert(0==getuid());
  assert(0==getgid());
  assert(0==seteuid(0));
  assert(0==setegid(0));
}

void privilege_drop(){
  assert(0==getuid());
  assert(0==getgid());
  const struct passwd *const pw=getpwnam(USR);
  assert(
    pw->pw_gid==1000&&
    pw->pw_uid==1000
  );
  assert(0==setegid(pw->pw_gid));
  assert(0==seteuid(pw->pw_uid));
}
