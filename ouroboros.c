/**
 * @file ouroboros.c
 * @brief Implementación de un buffer circular accesible mediante el sistema de archivos /proc.
 *
 * Este módulo crea un archivo virtual en /proc/ouroboros. Al escribir en él, 
 * se añaden cadenas al buffer; al leerlo, se extraen (consumen) una por una.
 */

#include "linux/kern_levels.h"
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#define PROC_NAME "ouroboros"
#define MAX_STRINGS 10      // Capacidad máxima del buffer
#define STR_SIZE 64         // Tamaño máximo de cada cadena

/* Estado Global */
static char circular_buffer[MAX_STRINGS][STR_SIZE];
static int head = 0;        // Índice para la próxima escritura
static int tail = 0;        // Índice para la próxima lectura (dato más antiguo)
static int line_count = 0;  // Contador de mensajes almacenados

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Román Contreras");
MODULE_DESCRIPTION("Buffer circular con soporte para procfs.");

/**
 * add_to_buffer - Lógica interna para la gestión del buffer circular.
 * @input: Cadena validada en espacio de kernel que se desea almacenar.
 */
static void add_to_buffer(const char *input) {

    // Copiamos la cadena asegurando la terminación nula
    strncpy(circular_buffer[head], input, STR_SIZE - 1);
    circular_buffer[head][STR_SIZE - 1] = '\0';

    // Avanzamos el índice de escritura
    head = (head + 1) % MAX_STRINGS;

    if (line_count < MAX_STRINGS) {
        line_count++;
    } else {
        // Modo sobrescritura: si el buffer está lleno, desplazamos tail
        // para mantener la estructura circular.
        tail = (tail + 1) % MAX_STRINGS;
    }

}

/**
 * proc_read - Callback para la llamada al sistema 'read' (ej. cat /proc/ouroboros).
 * @file: Objeto de archivo que representa la instancia abierta.
 * @usr_buf: Buffer de destino en el Espacio de Usuario.
 * @count: Cantidad máxima de bytes solicitados.
 * @pos: Offset actual del archivo.
 * * @return: Bytes copiados al usuario, o 0 para indicar fin de archivo (EOF).
 */
static ssize_t proc_read(struct file *file, char __user *usr_buf, size_t count, loff_t *pos) {
    char *target_str;
    int len;

    // Si pos > 0, el usuario ya leyó datos en esta sesión.
    // Esto evita que 'cat' entre en un bucle infinito.
    if (*pos > 0) return 0;


    if (line_count <= 0) {
        printk(KERN_ALERT "Buffer Ouroboros vacío.\n");
        return 0;
    }

    // Obtenemos el puntero a la cadena más antigua
    target_str = circular_buffer[tail];
    len = strlen(target_str);

    tail = (tail + 1) % MAX_STRINGS;
    line_count--;

    // Transferencia segura de Kernel a Usuario.
    if (copy_to_user(usr_buf, target_str, len))
        return -EFAULT;

    // Actualizamos la posición para que la siguiente llamada detecte EOF.
    *pos += len;

    printk(KERN_INFO "Lectura realizada en /proc/ouroboros.\n");
    return len;
}

/**
 * proc_write - Callback para la llamada al sistema 'write' (ej. echo "hola" > /proc/ouroboros).
 * @file: Objeto de archivo.
 * @usr_buf: Datos de origen en espacio de Usuario.
 * @count: Bytes enviados por el usuario.
 * @pos: Offset actual.
 * * @return: Cantidad de bytes procesados o código de error.
 */
static ssize_t proc_write(struct file *file, const char __user *usr_buf, size_t count, loff_t *pos) {
    char kbuf[STR_SIZE];
    // Truncamos la entrada para no exceder el tamaño de nuestras filas
    size_t len = min(count, (size_t)STR_SIZE - 1);

    // Traemos los datos desde el espacio de usuario de forma segura.
    if (copy_from_user(kbuf, usr_buf, len))
        return -EFAULT;

    kbuf[len] = '\0'; // Aseguramos el terminador nulo manualmente

    add_to_buffer(kbuf);

    printk(KERN_INFO "Escritura realizada en /proc/ouroboros.\n");
    return count;
}

/* Definición de la tabla de operaciones del archivo /proc */
static const struct proc_ops file_ops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

/**
 * proc_init - Punto de entrada del módulo (al cargar con insmod).
 */
static int __init proc_init(void) {
    // Permisos 0666: Lectura y escritura para todos los usuarios
    if (!proc_create(PROC_NAME, 0666, NULL, &file_ops)) {
        return -ENOMEM;
    }
    printk(KERN_INFO "Módulo cargado: /proc/%s creado.\n", PROC_NAME);
    return 0;
}

/**
 * proc_exit - Punto de salida del módulo (al descargar con rmmod).
 */
static void __exit proc_exit(void) {
    remove_proc_entry(PROC_NAME, NULL);
    printk(KERN_INFO "Módulo descargado: /proc/%s eliminado.\n", PROC_NAME);
}

module_init(proc_init);
module_exit(proc_exit);
