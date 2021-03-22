#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}


/*

sbrk() reserva las páginas físicas necesarias y 
las mapea en el espacio de direcciones virtuales del proceso.
 
Es decir aumenta el tamaño de la memoria de procesos en n bytes, 
y luego devuelve el inicio de la región recién asignada 
(es decir, el tamaño anterior).

Existen programas que reservan memoria pero nunca la utilizan.
Por esta razón los kernels suelen retrasar la reserva de cada página 
de la memoria hasta que la aplicación intenta usarla.

growproc(n) --> llama a allocuvm la cual reserva memoria
// Aumenta la memoria del proceso actual en n bytes.
// Devuelve 0 en caso de éxito, -1 en caso de error.

*/
int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;

  // Guardamos el tamano antiguo
  addr = myproc()->sz;

  // Si n<0 liberar los marcos mapeados
    if (n < 0){ //Ejercicio 2, en el caso de sys_brk reciba un argumento negativo
	if(growproc(n) < 0){
	 return -1;
	}
    } 
    else {  myproc()->sz += n;} //incrementamos el tamano del proceso sin reservar memoria
	
   return addr; // devolvemos el tamano antiguo
  

/*
  if(growproc(n) < 0)
    return -1;

  return addr;

*/

}




int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int sys_date(void)
{
    struct rtcdate*r;
    if ( argptr(0,(char**)&r,sizeof(struct rtcdate)) !=0){
        return -1;
}
    cmostime(r);
    return 0;
}

