#!/bin/bash
clang-format-8 -i -fallback-style=Webkit `find . -regex '.*\.[hc]$'` 
