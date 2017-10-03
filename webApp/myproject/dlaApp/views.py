# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.shortcuts import render

def visual(request):
	context = {"channels": [1,2,3,4,5], "wave":"1....0....1...0...1...0....1." }
	return render(request, "dlaApp/visual.html", context)

