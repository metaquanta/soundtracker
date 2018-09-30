#!/bin/bash
clang-format-6.0 -i -fallback-style=Webkit `find . -regex '.*\.[hc]$'` 
