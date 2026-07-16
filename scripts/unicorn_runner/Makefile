PY_INCLUDE := $(shell python3 -c "import sysconfig; print(sysconfig.get_path('include'))")
EXT_SUFFIX := $(shell python3 -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))")
UC_DIR     := $(HOME)/code/milohax/unicorn

TARGET := _trampoline_hook$(EXT_SUFFIX)

$(TARGET): _trampoline_hook.c
	gcc -shared -fPIC -O2 \
		-I$(PY_INCLUDE) -I$(UC_DIR)/include \
		-L$(UC_DIR)/build -Wl,-rpath,$(UC_DIR)/build \
		-o $@ $< -lunicorn

clean:
	rm -f $(TARGET)

.PHONY: clean
