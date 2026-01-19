# ==============================================================================
# MAKEFILE PRINCIPAL
# ==============================================================================

# Definimos los módulos. 
# UTILS va separado porque es una dependencia crítica.
UTILS_DIR = utils
MODULES = master worker storage query_control

# .PHONY indica que estos objetivos no son archivos físicos, sino acciones.
.PHONY: all clean $(UTILS_DIR) $(MODULES)

# 1. Regla por defecto: Compilar todo
all: $(UTILS_DIR) $(MODULES)

# 2. Definición de dependencias
# Esto asegura que 'utils' se compile ANTES que los otros módulos
$(MODULES): $(UTILS_DIR)

# 3. Regla para compilar utils
$(UTILS_DIR):
	@echo "\n========================================"
	@echo " COMPILANDO UTILS"
	@echo "========================================"
	@$(MAKE) -C $@

# 4. Regla para compilar el resto de módulos
$(MODULES):
	@echo "\n========================================"
	@echo " COMPILANDO $@"
	@echo "========================================"
	@$(MAKE) -C $@

# 5. Regla para limpiar todo (make clean)
clean:
	@echo "\n========================================"
	@echo " LIMPIANDO PROYECTO COMPLETO"
	@echo "========================================"
	@$(MAKE) -C $(UTILS_DIR) clean
	@for dir in $(MODULES); do \
		echo "-> Limpiando $$dir..."; \
		$(MAKE) -C $$dir clean; \
	done
	@echo "\n[OK] Limpieza finalizada."
