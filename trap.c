#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
extern int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);
uint ticks;



    /*
	Ejercicio 1

	Para responder a un fallo de página en el espacio de usuario
	hay que mapear una nueva página física en la dirección que generó el fallo,
	regresando después al espacio de usuario para que el proceso continúe.
	
	Para ello,reutilizaremos el siguiente código de allocuvm() 
	y PGROUNDDOWN(va) para redondear la dirección virtual a límite de página para coger la direccion de marco
.	
        La función allocuvm() se encarga de reservar las entradas en la
	tabla de páginas y las páginas físicas necesarias para que el proceso crezca de tamaño de oldsz a newsz
	Devuelve un nuevo tamaño o 0 en caso de error.

	--> nosotros solo queremos mapear una única página, no todas. Por lo que usaremos el codigo de allocuvm sin iterar

	Analizando el código de allocuvm,vemos que:

	    mem = kalloc(); //reserva memoria

	    if(mem == 0){ // Si no queda memoria para asignarle al proceso, lo notifica
	      cprintf("allocuvm out of memory\n");
	      return 0;
	    }

	    //limpia todo el bloque de memoria
	    memset(mem, 0, PGSIZE);

	    //mapea la pagina
	    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
	      cprintf("allocuvm out of memory (2)\n");
	      kfree(mem);
	      return 0;
	    }


	Donde:
		++ char * kalloc (void)
		// Asigna una página de 4096 bytes de memoria física.
		// Devuelve un puntero que el KERNEL puede usar.
		// Devuelve 0 si no se puede asignar la memoria.


		++ int mappages(pde_t *pgdir, void *VA, uint size, uint PA, int perm);
		// Mappages crea PTEs para direcciones virtuales que comienzan en VA que se refieren a direcciones físicas que comienzan en PA.
		   
		En nuestro caso:

			 mappages(myproc()->pgdir, (char*)PGROUNDDOWN(rcr2()), PGSIZE, V2P(bloque), PTE_W|PTE_U)
			
			  donde

				+myproc()->pgdir  	es la tabla de paginas

				+rcr2() --> guarda el valor de la direccion virtual genero que fallo de pagina
				
				+PGSIZE se define como  4096    // bytes mapped by a page

				+V2P(a) se define como 	(((uint) (a)) - KERNBASE)

				+Con los permisos : PTE_W           0x002   // Writeable
					            PTE_U           0x004   // User

			

	Ejercicio 2-> Se nos pide que tengamos en cuenta las siguientes consideraciones:

		-El caso de un argumento negativo al llamarse a sbrk 
		--> Hemos anadido una comprobacion en sysproc.c

		-Manejar el caso de fallos en la página inválida debajo de la pila.
		--> 

			exec() coloca una página inaccesible justo debajo de la página de pila
			Los programas que intenten usar más de una página fallarán

			Para evitar que se acceda a dicha página, la recuperaremos añadiendo un campo a proc.h
			Y luego comprobaremos que la pagina que ha dado el fallo no sea esta pagina inaccesible
			
			
		-Verificar que fork() y exit()/wait() funciona en el caso de que 
		haya direcciones virtuales sin memoria reservada para ellas.
		-->
			En el fichero vm.c hemos sustituido dejado pasarlos “panics” de la función copyuvm(), 
			ya que estos errores relacionados con el fallo de página 
			los estaríamos solucionando en el fichero trap.c
		   
			

		-Asegurarse de que funciona  el uso por parte del kernel
		de páginas de usuario que todavía no han sido reservadas 
		(p.e., si un programa pasa una dirección de la zona de usuario todavía no reservada a read())
		-->

			Para ello en la comprobación if ((tf->cs&3) == 0){
			realizaremos el mismo tratamiento que para el fallo de página
	

    */

    

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:

    //if(myproc() == 0 || (tf->cs&3) == 0){

    if(myproc() == 0){ 
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    
    // cs selector de segmento de codigo de usuario 
    else if ((tf->cs&3) == 0){ 

		//Vemos si el valor de la página que provocó el fallo se corresponde con la página inaccesible que habíamos guardado
		if ( PGROUNDDOWN(rcr2()) == myproc()->paginaInvalida ){ 
			myproc()->killed = 1; //Matamos el proceso si lo es
			break;
	    	}

		// Intentamos reservar una página de 4096 bytes de memoria física
		char *bloque = kalloc();

		
        	if (bloque==0){ // Si no queda memoria para asignarle al proceso, lo eliminamos
                	cprintf("Out of memory\n");
       			myproc()->killed = 1;
		   	break;
            	}

		// Limpiamos la página inicializandola a 0
	   	memset(bloque, 0, PGSIZE);


		//Mapeamos la página de memoria fisica reservada a la pagina virtual que ha dado el fallo
	    	if (mappages(myproc()->pgdir, (char*)PGROUNDDOWN(rcr2()), PGSIZE, V2P(bloque), PTE_W|PTE_U) < 0){
			cprintf("Out of memory (2)\n");
			myproc()->killed = 1; //Si no es posible, matamos el proceso
			kfree(bloque); // liberamos la memoria que habiamos reservado
			break;
	   	}

    }

    // Si la excep, ha sido la de fallo de pagina (T_PGFLT)
    // -> mapeo la dir.virtual a una nueva pagina fisica
    else if (tf->trapno == T_PGFLT){

		// Ejercicio 1 -clase
		//char* bloque = kalloc();
		//mappages(myproc()->pgdir, (void*)PGROUNDDOWN(rcr2()), PGSIZE, V2P(bloque), PTE_W|PTE_U);


		//Vemos si el valor de la página que provocó el fallo se corresponde con la página inaccesible que habíamos guardado
		if ( PGROUNDDOWN(rcr2()) == myproc()->paginaInvalida ){ 
			myproc()->killed = 1; //Matamos el proceso si lo es
			break;
	    	}


		// Intentamos reservar una página de 4096 bytes de memoria física
		char *bloque = kalloc();

		
        	if (bloque==0){ // Si no queda memoria para asignarle al proceso, lo eliminamos
                	cprintf("Out of memory\n");
       			myproc()->killed = 1;
		   	break;
            	}

		// Limpiamos la página inicializandola a 0
	   	memset(bloque, 0, PGSIZE);


		//Mapeamos la página de memoria fisica reservada a la pagina virtual que ha dado el fallo
	    	if (mappages(myproc()->pgdir, (char*)PGROUNDDOWN(rcr2()), PGSIZE, V2P(bloque), PTE_W|PTE_U) < 0){
			cprintf("Out of memory (2)\n");
			myproc()->killed = 1; //Si no es posible, matamos el proceso
			kfree(bloque); // liberamos la memoria que habiamos reservado
			break;
	   	}


         }//else if (tf->trapno == T_PGFLT)

	 // In user space, assume process misbehaved.
         else{
    		cprintf("pid %d %s: trap %d err %d on cpu %d "
            	"eip 0x%x addr 0x%x--kill proc\n",
            	myproc()->pid, myproc()->name, tf->trapno,
            	tf->err, cpuid(), tf->eip, rcr2());
    		myproc()->killed = 1;
		break;
         }

  }//else if



  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
