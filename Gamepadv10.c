#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <linux/uinput.h>
#include <math.h>
#include <linux/joystick.h>

// Identificación del joystick, cambiar esto si es necesario
#define JOYSTICK_DEVNAME "/dev/input/js0"

static int joystick_fd = -1;

typedef struct{
    int _x;
    int _y;
} Coordenadas;

static int uinput_fd = 0;
static Coordenadas coord[2];

static int num_of_axis=0, num_of_buttons=0;
static char *button=NULL, name_of_joystick[80];

// Array para almacenar captura de teclado
int Boton_X [255];
int Boton_C [255];
int Boton_T [255];
int Boton_O [255];
int AUX [255];


// Variables globales
int programacion = 0;
int cont = 0;
int fd_teclado = -1;
int esperoTecla = 0;

struct input_event eventos[64];

// Abre el joystick en modo lectura, esto evita cambiar los permisos
int open_joystick()
{
	joystick_fd = open(JOYSTICK_DEVNAME, O_RDONLY | O_NONBLOCK);
	if (joystick_fd < 0)
		return joystick_fd;

	// IOCTL permite cambiar y leer la configuracion del dispositivo
	ioctl( joystick_fd, JSIOCGAXES, &num_of_axis );
	ioctl( joystick_fd, JSIOCGBUTTONS, &num_of_buttons );
	ioctl( joystick_fd, JSIOCGNAME(80), &name_of_joystick );
	num_of_axis = num_of_axis & 0xFF;
	num_of_buttons = num_of_buttons & 0xFF;


	button = (char *) calloc( num_of_buttons, sizeof( char ) );
	return joystick_fd;
}

int read_joystick_event(struct js_event *jse)
{
	int bytes;

	bytes = read(joystick_fd, jse, sizeof(*jse)); 

	if (bytes == -1)
		return 0;

	if (bytes == sizeof(*jse))
		return 1;

	printf("Unexpected bytes from joystick:%d\n", bytes);

	return -1;
}

void close_joystick()
{
	close(joystick_fd);
}

int get_joystick_status(int *id)
{
	int rc;
	struct js_event jse;
	if (joystick_fd < 0)
		return -1;

	// memset(wjse, 0, sizeof(*wjse));
	while ((rc = read_joystick_event(&jse) == 1)) {
		jse.type &= ~JS_EVENT_INIT; /* ignore synthetic events */
         printf("time: %9u  value: %6d  type: %3u  number:  %2u\r",
				 jse.time, jse.value, jse.type, jse.number);
		     fflush(stdout);
		if (jse.type == JS_EVENT_AXIS) {
			switch (jse.number) {
			case 0: coord[0]._x = jse.value;
			*id = 0;
			break;
			case 1: coord[0]._y = jse.value;
			*id = 0;
			break;
			case 2: coord[1]._x = jse.value;
			*id = 1;
			break;
			case 3: coord[1]._y  = jse.value;
			*id = 1;
			break;
			default:
				break;
			}
			return JS_EVENT_AXIS;
		} else if (jse.type == JS_EVENT_BUTTON) {
			button [jse.number] = jse.value;
			*id = jse.number;
			return JS_EVENT_BUTTON;
		}

		return 0;
	}
	return 0;
}

int uinput_mouse_init(void) {
	struct uinput_user_dev dev;
	int i;

	uinput_fd = open("/dev/uinput", O_WRONLY);
	if (uinput_fd <= 0) {
		perror("Error opening the uinput device\n");
		return -1;
	}
	memset(&dev,0,sizeof(dev)); // Initialize the uInput device to NULL
	strncpy(dev.name, "mouse", UINPUT_MAX_NAME_SIZE);
	dev.id.version = 4;
	dev.id.bustype = BUS_USB;
	dev.id.product = 1;
	ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
	ioctl(uinput_fd, UI_SET_EVBIT, EV_REL);
	ioctl(uinput_fd, UI_SET_RELBIT, REL_X);
	ioctl(uinput_fd, UI_SET_RELBIT, REL_Y);
	ioctl(uinput_fd, UI_SET_KEYBIT, BTN_MOUSE);
	ioctl(uinput_fd, UI_SET_KEYBIT, BTN_LEFT);
	ioctl(uinput_fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(uinput_fd, UI_SET_KEYBIT, KEY_LEFTMETA);


	for (i=0; i<256; i++) {
		ioctl(uinput_fd, UI_SET_KEYBIT, i);
	}


	write(uinput_fd, &dev, sizeof(dev));
	if (ioctl(uinput_fd, UI_DEV_CREATE))
	{
		printf("Unable to create UINPUT device.");
		return -1;
	}
	return 1;
}

void uinput_mouse_move_cursor(int x, int y )
{
	float theta;
	struct input_event event; // Input device structure

    // obtiene el angulo de movimiento
    if (x == 0 && y == 0)
    	return;
    else if  (x == 0)
    	theta = M_PI/2;
    else
       theta = atan (abs(y/x));

    printf(" x : %i, y : %i theta : %f\n",x,y,theta);
    
	float xdir = cos(theta);
	float ydir = sin(theta);
	if (x < 0)
	   xdir *= -1;
	if (y < 0)
	    ydir *= -1;

	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = EV_REL;
	event.code = REL_X;
	event.value = xdir*10;
	write(uinput_fd, &event, sizeof(event));
	event.type = EV_REL;
	event.code = REL_Y;
	event.value = ydir*10;
	write(uinput_fd, &event, sizeof(event));
	event.type = EV_SYN;
	event.code = SYN_REPORT;
	event.value = 0;
	write(uinput_fd, &event, sizeof(event));
}

void press_left()
{
	// Report BUTTON CLICK - PRESS event
	struct input_event event; // Input device structure
	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = EV_KEY;
	event.code = BTN_LEFT;
	event.value = 1;
	write(uinput_fd, &event, sizeof(event));
	event.type = EV_SYN;
	event.code = SYN_REPORT;
	event.value = 0;
	write(uinput_fd, &event, sizeof(event));
}


void release_left()
{
	// Report BUTTON CLICK - RELEASE event
	struct input_event event; // Input device structure
	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = EV_KEY;
	event.code = BTN_LEFT;
	event.value = 0;
	write(uinput_fd, &event, sizeof(event));
	event.type = EV_SYN;
	event.code = SYN_REPORT;
	event.value = 0;
	write(uinput_fd, &event, sizeof(event));
}

// Boton derecho

void press_right()
{
	// Report BUTTON CLICK - PRESS event
	struct input_event event; // Input device structure
	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = EV_KEY;
	event.code = BTN_RIGHT;
	event.value = 1;
	write(uinput_fd, &event, sizeof(event));
	event.type = EV_SYN;
	event.code = SYN_REPORT;
	event.value = 0;
	write(uinput_fd, &event, sizeof(event));
}

void release_right()
{
	// Report BUTTON CLICK - RELEASE event
	struct input_event event; // Input device structure
	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = EV_KEY;
	event.code = BTN_RIGHT;
	event.value = 0;
	write(uinput_fd, &event, sizeof(event));
	event.type = EV_SYN;
	event.code = SYN_REPORT;
	event.value = 0;
	write(uinput_fd, &event, sizeof(event));
}

// Boton HOME 

void press_home()
{
	// Report BUTTON CLICK - PRESS event
	struct input_event event; // Input device structure
	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = EV_KEY;
	event.code = KEY_LEFTMETA;
	event.value = 1;
	write(uinput_fd, &event, sizeof(event));
	event.type = EV_SYN;
	event.code = SYN_REPORT;
	event.value = 0;
	write(uinput_fd, &event, sizeof(event));
}


void release_home()
{
	// Report BUTTON CLICK - RELEASE event
	struct input_event event; // Input device structure
	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = EV_KEY;
	event.code = KEY_LEFTMETA;
	event.value = 0;
	write(uinput_fd, &event, sizeof(event));
	event.type = EV_SYN;
	event.code = SYN_REPORT;
	event.value = 0;
	write(uinput_fd, &event, sizeof(event));
}


// MODO PROGRAMACION


void start_programming(){
    fflush(stdout);     //Limpiamos el buffer
    printf("Comenzamos a leer\n");

	fd_teclado = open("/dev/input/event3", O_RDONLY|O_NDELAY);    //Introduciomos O_NONBLOCK para que no se bloquee en read()
	if (fd_teclado == -1){
		printf ("ERROR de apertura de teclado\n");
	}    
    
    for(int i = 0; i<255; i++){ //Limpiamos AUX para leer nuevos eventos
        AUX[i] = 0;
    }  
	cont = 0;
	programacion = 1;
}

void end_programming(){
    fflush(stdout);     //Limpiamos el buffer
	programacion = 0;

	if( close(fd_teclado) < 0)
        perror("Fallo en el close del teclado");
	fd_teclado = -1;

	esperoTecla = 1;
}

int  leeTecla(){
		
	int bytes_read = 0;
	int num_events;
	if (!programacion)	return -1;

	// Leemos un evento del fichero del dispositivo.
	if (fd_teclado <  0) exit(1);
	bytes_read = read(fd_teclado, &eventos, sizeof(struct input_event)* 64);

	if (bytes_read <= 0) return -1;

	// Comprobamos que la lectura es (al menos) un evento completo.
	if ( bytes_read >= (int) sizeof(struct input_event)){      
		num_events=bytes_read / sizeof(struct input_event);			 

		for (int i = 0; i < num_events; i++){   			  	
			//Ver el tipo y código, por si tenemos curiosidad ...					
			//printf("Evento de tipo (%d) - codigo (%d) \n",eventos[i].type, eventos[i].code);      
			
            if( eventos[i].type == EV_KEY && eventos[i].value == 1 && eventos[i].code != 4 && eventos[i].code != 0 ){
                AUX[cont] = eventos[i].code;
                    printf("Guardo tecla\n");
			        printf("N: %d\n",eventos[i].code);
                cont++;
            }

			/*
                Insertamos un filtro para quedarnos solo con los eventos que nos interesan:
                    -Eventos de teclado -> EV_KEY
                    -Escogemos solo el release (ya que se generaban eventos tanto de press() como release())
                    -Los que su codigo no sean ni 4 ni 0, ya que eran los que se repetian siempre
                De este modo sólo almacenamos los eventos de teclado que teniamos entre medio del 4 y 0, es decir,
                almacenamos el codigo de la tecla que pulsamos.
			*/			
		}
	} 

	
    return 0;
}


// La funcion recibe el codigo de una tecla y simula press/realease

void escribeTeclado (int idTecla){
    struct input_event event; // Input device structure
	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = EV_KEY;
	event.code = idTecla;
	event.value = 1;
	write(uinput_fd, &event, sizeof(event));
    event.value = 0;
    write(uinput_fd, &event, sizeof(event));
	event.type = EV_SYN;
	event.code = SYN_REPORT;
	event.value = 0;
	write(uinput_fd, &event, sizeof(event));
}

// La función selecciona un boton y va escribiendo la secuencia que tiene asignada

void secuenciaBoton(int id){

	if(id == 3){		//CUADRADO
		//muestra botonC
		printf("\n");
		for (int i = 0; i<255; i++){
			if(Boton_C[i] != 0){
	                escribeTeclado(Boton_C[i]);
			} else { //Supongo que ninguna tecla del teclado va a tener el id 0
				break;
			}
		}
		printf("\n");
	}else if(id == 2){	//TRIANGULO	
		printf("\n");
		for (int i = 0; i<255; i++){
			if(Boton_T[i] != 0){
                escribeTeclado(Boton_T[i]);
			} else { //Supongo que ninguna tecla del teclado va a tener el id 0
				break;
			}
		}
		printf("\n");
	}else if(id == 1){  //CIRCULO
		printf("\n");
		for (int i = 0; i<255; i++){
			if(Boton_O[i] != 0){
				//printf("%d \t", Boton_O[i]);
                escribeTeclado(Boton_O[i]);
			} else { //Supongo que ninguna tecla del teclado va a tener el id 0
				break;
			}
		}
		printf("\n");
	}else if(id == 0){	//X
		printf("\n");
		for (int i = 0; i<255; i++){
			if(Boton_X[i] != 0){
				//printf("%d \t", Boton_X[i]);
                escribeTeclado(Boton_X[i]);
			} else { //Supongo que ninguna tecla del teclado va a tener el id 0
				break;
			}
		}
		printf("\n");
	}else{
		printf("Tecla no valida\n");
	}
}


/* a little test program */
int main(int argc, char *argv[])
{
	int fd;
	int status;
	int id;
	char mueve;
	char contador;

	fd = open_joystick();

	if (fd < 0) {
		printf("open failed.\n");
		exit(1);
	}

	if (uinput_mouse_init()<0)
	{
		printf("open failed.\n");
		exit(2);
	}
	mueve = 0;


	while (1) {

		usleep(1000);
		if(programacion == 1)
        {
            leeTecla();
        }
		if (mueve)
		{
			if (contador > 100)
			{
			   uinput_mouse_move_cursor(coord[id]._x,coord[id]._y);
			   contador = 0;
			}
			contador++;
		}
		status = get_joystick_status(&id);

		if (status <= 0)
			continue;
		if (status == JS_EVENT_BUTTON)
		{
			if (id == 0){ //Pulsamos Equis
				if (button[id] == 1){
					//Volcamos AUX en el array de Equis
					
					if (esperoTecla == 1){
						for(int i=0; i<255; i++){
							Boton_X[i] = AUX[i];
						}
						printf("Teclas capturadas asociadas al Boton Equis\n");
						esperoTecla = 0;
					}else{
						secuenciaBoton(id);

					}
				}
			} 
			else if (id == 1) //Pulsamos Circulo
			{ 
				if (button[id] == 1){
					//Volcamos AUX en el array de Circulo
					
					if (esperoTecla == 1){
						for(int i=0; i<255; i++){
							Boton_O[i] = AUX[i];
						}
						printf("Teclas capturadas asociadas al Boton Circulo\n");
						esperoTecla = 0;
					}else{
						secuenciaBoton(id);

					}
				}
			} 
			else if (id == 2) //Pulsamos Triangulo
			{ 
				if (button[id] == 1){
					//Volcamos AUX en el array de Triangulo
				
					if (esperoTecla == 1){
						for(int i=0; i<255; i++){
							Boton_T[i] = AUX[i];
						}
						printf("Teclas capturadas asociadas al Boton Triangulo\n");
						esperoTecla = 0;
					}else{
						secuenciaBoton(id);

					}
				}

			} 
			else if (id == 3)       // Pulsamos Cuadrado
            {
				if (button[id] == 1){
					//Volcamos AUX en el array del cuadrado
				
					if (esperoTecla ==1){
						for(int i=0; i<255; i++){
							Boton_C[i] = AUX[i];
						}
						printf("Teclas capturadas asociadas al Boton Cuadrado\n");
						esperoTecla = 0;
					}else{
						secuenciaBoton(id);
					}
				}
            }
			else if (id == 4)       // Pulsamos L1
			{
				if (button[id] == 1)
					press_left();
				else
					release_left();
			}
            else if (id == 5)       // Pulsamos R1
			{
				if (button[id] == 1)
					press_right();
				else
					release_right();
			}
            else if (id == 10)       // Pulsamos HOME
			{
				if (button[id] == 1)
					press_home();
				else
					release_home();
			}
            else if (id == 6)       // Pulsamos L2
			{
				if (button[id] == 1)
					start_programming();
			}
            else if (id == 7)      // Pulsamos R2
			{
				if (button[id] == 1)
					end_programming();      //Cerramos el teclado
			}
		}
		else if (status == JS_EVENT_AXIS)
		{
            // Captura información del controlador digital.
			if (id == 0)
			{
				if (coord[id]._x != 0 || coord[id]._y != 0)
					mueve = 1;
				else
					mueve = 0;
			}
		}
	}
}