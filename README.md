# Master of Files
**Sistemas Operativos - UTN FRBA - 2C 2025**

![Language](https://img.shields.io/badge/Language-C-blue)
![Platform](https://img.shields.io/badge/Platform-Linux-orange)
![Course](https://img.shields.io/badge/UTN-Sistemas%20Operativos-green)

## Grupo: Crankzaharphite 2.0
- Genaro Melitón Clemente
- Lucca Polastri
- Gonzalo Galarza
- Antu Ponturo
- Valentín Reyes

---

## Descripción
Implementación de un sistema distribuido que simula la planificación y ejecución de procesos sobre un File System propio. El sistema está compuesto por cuatro módulos principales:

* **Master:** Orquestador y planificador (Kernel).
* **Storage:** Sistema de Archivos distribuido (manejo de bloques, bitmaps y persistencia).
* **Worker:** Ejecutor de tareas (CPU/Memoria).
* **Query Control:** Consola cliente para envío de solicitudes.

---

## Compilación y Dependencias

### Prerrequisitos
El proyecto requiere las `commons-library` de la cátedra. Si no las tienes instaladas ejecute el script install_libs.sh del directorio scripts.


git clone [https://github.com/sisoputnfrba/so-commons-library](https://github.com/sisoputnfrba/so-commons-library)
cd so-commons-library
make install
