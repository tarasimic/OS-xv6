// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"


static void consputc(int);

static int panicked = 0, bool=1;

static struct {
	struct spinlock lock;
	int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
	static char digits[] = "0123456789abcdef";
	char buf[16];
	int i;
	uint x;

	if(sign && (sign = xx < 0))
		x = -xx;
	else
		x = xx;

	i = 0;
	do{
		buf[i++] = digits[x % base];
	}while((x /= base) != 0);

	if(sign)
		buf[i++] = '-';

	while(--i >= 0)
		consputc(buf[i]);
}

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
	int i, c, locking;
	uint *argp;
	char *s;

	locking = cons.locking;
	if(locking)
		acquire(&cons.lock);

	if (fmt == 0)
		panic("null fmt");

	argp = (uint*)(void*)(&fmt + 1);
	for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
		if(c != '%'){
			consputc(c);
			continue;
		}
		c = fmt[++i] & 0xff;
		if(c == 0)
			break;
		switch(c){
		case 'd':
			printint(*argp++, 10, 1);
			break;
		case 'x':
		case 'p':
			printint(*argp++, 16, 0);
			break;
		case 's':
			if((s = (char*)*argp++) == 0)
				s = "(null)";
			for(; *s; s++)
				consputc(*s);
			break;
		case '%':
			consputc('%');
			break;
		default:
			// Print unknown % sequence to draw attention.
			consputc('%');
			consputc(c);
			break;
		}
	}

	if(locking)
		release(&cons.lock);
}

void
panic(char *s)
{
	int i;
	uint pcs[10];

	cli();
	cons.locking = 0;
	// use lapiccpunum so that we can call panic from mycpu()
	cprintf("lapicid %d: panic: ", lapicid());
	cprintf(s);
	cprintf("\n");
	getcallerpcs(&s, pcs);
	for(i=0; i<10; i++)
		cprintf(" %p", pcs[i]);
	panicked = 1; // freeze other CPU
	for(;;)
		;
}

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

int pos, temp;
static void
cgaputc(int c)
{

	// Cursor position: col + 80*row.
	outb(CRTPORT, 14);
	pos = inb(CRTPORT+1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT+1);
	
	if(c == '\n')
		pos += 80 - pos%80;
	else if(c == BACKSPACE){
		if(pos > 0) --pos;
	} else {
		
		crt[pos++] = (c&0xff) | 0x0700;  // black on white
	}
	

 	if(pos < 0 || pos > 25*80)
		panic("pos under/overflow");

	if((pos/80) >= 24){  // Scroll up.
		memmove(crt, crt+80, sizeof(crt[0])*23*80);
		pos -= 80;
		memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
	}
	

	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	crt[pos] = ' ' | 0x0700;
	
}

void
consputc(int c)
{
	if(panicked){
		cli();
		for(;;)
			;
	}

	if(c == BACKSPACE){
		uartputc('\b'); uartputc(' '); uartputc('\b');
	} else
		uartputc(c);
	cgaputc(c);
}

#define INPUT_BUF 128
struct {
	char buf[INPUT_BUF];
	uint r;  // Read index
	uint w;  // Write index
	uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x
#define A(x) ((x)+';')

int flag = 0, temp, check = 0, pozicija, pozicija1,  position, count = 0, temp_, temp1, temp2 = 0, temp3, position1, temp4 = 0;
int tempW;

void
consoleintr(int (*getc)(void))
{
	int c, doprocdump = 0;

	acquire(&cons.lock);
	while((c = getc()) >= 0){
		switch(c){
		case C('P'):  // Process listing.
			// procdump() locks cons.lock indirectly; invoke later
			break;
			doprocdump = 1;
		case C('U'):  // Kill line.
			while(input.e != input.w &&
			      input.buf[(input.e-1) % INPUT_BUF] != '\n'){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case C('H'): case '\x7f':  // Backspace
			if(input.e != input.w){
				input.e--;
				consputc(BACKSPACE);
			}
			break;

		case A('C'):
			bool = !bool;
			check = !check;
			if(check){
				temp = pos;

			}
			if(bool){
				if(!check){
					crt[pos] = (crt[pos]&0xff) | 0x0700;
					pos = temp;
					outb(CRTPORT, 14);
					outb(CRTPORT+1, pos>>8);
					outb(CRTPORT, 15);
					outb(CRTPORT+1, pos);
					crt[pos] =  ' ' | 0x0700;
					check = 1; 
				}
			}
			break;
		case A('P'):
			for(int i = pozicija; i <= pozicija1; i++){
				outb(CRTPORT, 14);
				outb(CRTPORT+1, pos>>8);
				outb(CRTPORT, 15);
				outb(CRTPORT+1, pos);
				crt[pos-1] = (crt[i-1]&0xff) | 0x0700;
				pos++;
			}
			break;
		default:
			if(c != 0 && input.e-input.r < INPUT_BUF){
				c = (c == '\r') ? '\n' : c;
				input.buf[input.e++ % INPUT_BUF] = c;

				if(bool){
					cgaputc(c);

				}else {
					if(!flag && !bool){
						if(c == 'w'){
							
							crt[pos] = (crt[pos]&0xff) | 0x0700; 
							pos-=80;
							crt[pos] = (crt[pos]&0xff) | 0x7000;  
							outb(CRTPORT, 14);
							outb(CRTPORT+1, pos>>8);
							outb(CRTPORT, 15);
							outb(CRTPORT+1, pos);
							
						}
					
						if(c == 'd'){
							crt[pos] = (crt[pos]&0xff) | 0x0700;
							pos++;
							crt[pos] = (crt[pos]&0xff) | 0x7000; 
							outb(CRTPORT, 14);
							outb(CRTPORT+1, pos>>8);
							outb(CRTPORT, 15);
							outb(CRTPORT+1, pos);
							
						}
					
						if(c == 'a'){
							crt[pos] = (crt[pos]&0xff) | 0x0700;
							pos--;
							crt[pos] = (crt[pos]&0xff) | 0x7000; 
							outb(CRTPORT, 14);
							outb(CRTPORT+1, pos>>8);
							outb(CRTPORT, 15);
							outb(CRTPORT+1, pos);
							//crt[pos] = ' ' | 0x7000;
						}
					
						if(c == 's'){
							crt[pos] = (crt[pos]&0xff) | 0x0700;
							pos+=80;
							crt[pos] = (crt[pos]&0xff) | 0x7000; 
							outb(CRTPORT, 14);
							outb(CRTPORT+1, pos>>8);
							outb(CRTPORT, 15);
							outb(CRTPORT+1, pos);
							//crt[pos] = ' ' | 0x7000;
						}
					
					}
					if(c == 'q'){
						crt[pos] = (crt[pos]&0xff) | 0x0700;
						flag = 1;
						pozicija = pos;
					}
					if(flag){
						if(c == 'd'){
							pos++;
							crt[pos-1] = (crt[pos-1]&0xff) | 0x7000;
							outb(CRTPORT, 14);
							outb(CRTPORT+1, pos>>8);
							outb(CRTPORT, 15);
							outb(CRTPORT+1, pos);
							
						}
						
						if(c == 'a'){
							if(pos >= pozicija){
								crt[pos-1] = (crt[pos-1]&0xff) | 0x0700;
								pos--;
								outb(CRTPORT, 14);
								outb(CRTPORT+1, pos>>8);
								outb(CRTPORT, 15);
								outb(CRTPORT+1, pos);

							}
							
							
						}

						if(c == 'w'){
							if(!temp4){
								count++;
								if(count == 1){
									for(int i = pozicija; i < pos; i++){
										crt[i] = (crt[i]&0xff) | 0x0700;
									}
									pos-=80;
									temp = pos;
									outb(CRTPORT, 14);
									outb(CRTPORT+1, pos>>8);
									outb(CRTPORT, 15);
									outb(CRTPORT+1, pos);
									while(temp < pozicija){
										crt[temp] = (crt[temp]&0xff) | 0x7000;
										crt[temp++];
									}

								}else if(count > 1){
									pos-=80;
									temp_ = pos;
									temp-=80;
									outb(CRTPORT, 14);
									outb(CRTPORT+1, pos>>8);
									outb(CRTPORT, 15);
									outb(CRTPORT+1, pos);
									for(int i = temp; i < pozicija; i++){
										crt[i] = (crt[i]&0xff) | 0x7000;
									}
									while(temp_ < temp){
										crt[temp_] = (crt[temp_]&0xff) | 0x7000;
										crt[temp_++];
									}

									temp2 = 1;
									
								}
							}else if(temp){
								tempW = pos;
								pos-=80;
								outb(CRTPORT, 14);
								outb(CRTPORT+1, pos>>8);
								outb(CRTPORT, 15);
								outb(CRTPORT+1, pos);
								while(tempW >= pos){
									crt[tempW] = (crt[tempW]&0xff) | 0x0700;
									crt[tempW--];
								}
							}
							
						}

						if(c == 's'){
							temp4 = 1;
							if(temp2){
								temp3 = pos;
								pos+=80;
								outb(CRTPORT, 14);
								outb(CRTPORT+1, pos>>8);
								outb(CRTPORT, 15);
								outb(CRTPORT+1, pos);
								for(int i = temp3; i < pos; i++){
									crt[i] = (crt[i]&0xff) | 0x0700;
								}
								temp2 = 0;
							}else if(!temp2){
								temp1 = pos;
								pos+=80;
								outb(CRTPORT, 14);
								outb(CRTPORT+1, pos>>8);
								outb(CRTPORT, 15);
								outb(CRTPORT+1, pos);
								for(int i = temp1; i < pos; i++){
									crt[i] = (crt[i]&0xff) | 0x7000;
								}
							}
						}

						if(c == 'e'){
							
							pozicija1 = pos;
							for(int i = pozicija; i < pozicija1; i++){
								crt[i] = (crt[i]&0xff) | 0x0700;
							}
						}	
							
					}
				}

				
				if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
					input.w = input.e;
					wakeup(&input.r);
				}
			}
			break;
		}
	}
	release(&cons.lock);
	if(doprocdump) {
		procdump();  // now call procdump() wo. cons.lock held
	}
}

int
consoleread(struct inode *ip, char *dst, int n)
{
	uint target;
	int c;

	iunlock(ip);
	target = n;
	acquire(&cons.lock);
	while(n > 0){
		while(input.r == input.w){
			if(myproc()->killed){
				release(&cons.lock);
				ilock(ip);
				return -1;
			}
			sleep(&input.r, &cons.lock);
		}
		c = input.buf[input.r++ % INPUT_BUF];
		if(c == C('D')){  // EOF
			if(n < target){
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
			}
			break;
		}
		*dst++ = c;
		--n;
		if(c == '\n')
			break;
	}
	release(&cons.lock);
	ilock(ip);

	return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{ 
	int i;

	iunlock(ip);
	acquire(&cons.lock);
	for(i = 0; i < n; i++)
		consputc(buf[i] & 0xff);
	release(&cons.lock);
	ilock(ip);

	return n;
}

void
consoleinit(void)
{
	initlock(&cons.lock, "console");

	devsw[CONSOLE].write = consolewrite;
	devsw[CONSOLE].read = consoleread;
	cons.locking = 1;

	ioapicenable(IRQ_KBD, 0);
}

