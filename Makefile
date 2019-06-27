all: setup_var
setup_var: setup_var.o
clean:
	rm setup_var setup_var.o
