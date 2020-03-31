AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4
SUBDIRS = libubx std_blocks std_types lua tools examples

docs:
	@$(MAKE) -C docs html SPHINXBUILD=$(SPHINXBUILD)

cppcheck:
	@cppcheck -q \
		  --error-exitcode=255 \
		  --enable=all \
		  -Ilibubx/ -DRAMP_T=int \
		  --suppress=missingInclude \
		  --suppress=unusedFunction \
		  --suppress=*:std_blocks/webif/mongoose.c \
		  --suppress=variableScope:libubx/md5.c . || (echo "cppcheck failed $$?"; exit 255)
