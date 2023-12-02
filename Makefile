define compile_run
	@echo ---------
	g++ -Wall -Wextra -Werror -o a.out$(1) $(2) -std=c++17 main.cpp -pthread 2>&1 && ./a.out$(1)
	@echo ---------
	@echo
endef

all: main.cpp *.hpp
	$(call compile_run,,-O3)
	$(call compile_run,.debug,-g3)
	$(call compile_run,.address,-g3   -fsanitize=address  )
	$(call compile_run,.thread,-g3    -fsanitize=thread   )
	$(call compile_run,.undefined,-g3 -fsanitize=undefined)

clean:
	rm -f a.out a.out.*
