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

## Dependencias

### Prerrequisitos
El proyecto requiere las `commons-library` de la cátedra. Si no las tienes instaladas ejecute el script install_libs.sh del directorio scripts.

## Ejecución y Pruebas

Para facilitar el despliegue, el proyecto cuenta con una suite de scripts automatizados que se encargan de **compilar, limpiar logs anteriores y ejecutar todos los módulos** en el orden correcto según el escenario de prueba.

No es necesario levantar los procesos manualmente.

### Instrucciones
Simplemente ejecuta cualquiera de los scripts con el prefijo `run_test_` que se encuentran en la carpeta scripts:

1. **Dar permisos de ejecución** (solo la primera vez):
   ```bash
   chmod +x run_test_*.sh
   ```
2. Ejecutar un escenario de prueba: Elige el test que desees correr. Por ejemplo:
   ```bash
   ./run_test_errores.sh
   ./run_test_estabilidad.sh
   ./run_test_memoria_worker.sh
   ```
   El script se encargará de finalizar todos los proceso al terminal la prueba
   
