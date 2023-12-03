define compile_run
	@echo ---------
	@mkdir -p out
	g++ -I./include -Wall -Wextra -Werror -o out/a.out$(1) $(2) -std=c++17 test/main.cpp -pthread 2>&1 && ./out/a.out$(1)
	@echo ---------
	@echo
endef

all: test/main.cpp
	$(call compile_run,,-O3)
	$(call compile_run,.debug,-g3)
	$(call compile_run,.address,-g3   -fsanitize=address  )
	$(call compile_run,.thread,-g3    -fsanitize=thread   )
	$(call compile_run,.undefined,-g3 -fsanitize=undefined)
	@echo PASSED

clean:
	rm -f out/a.out out/a.out.*
