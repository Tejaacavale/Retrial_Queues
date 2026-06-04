from setuptools import setup, Extension
import pybind11

ext = Extension(
    "retrial_queue",
    sources=["bindings.cpp"],
    include_dirs=[pybind11.get_include()],
    extra_compile_args=["-std=c++11", "-O2"],
    language="c++",
)

setup(
    name="retrial-queue",
    version="0.1.0",
    description="M/M/1 retrial queue simulator (C++ core, Python bindings)",
    ext_modules=[ext],
)
