# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('spy', ['location-service', 'internet', 'wifi', 'applications', 'mesh', 'point-to-point', 'virtual-net-device'])
    module.source = [
        'model/spy-ptable.cc',
        'model/spy-rqueue.cc',
        'model/spy-packet.cc',
        'model/spy.cc',
        'helper/spy-helper.cc',
        ]

    headers = bld(features='ns3header')
    headers.module = 'spy'
    headers.source = [
        'model/spy-ptable.h',
        'model/spy-rqueue.h',
        'model/spy-packet.h',
        'model/spy.h',
        'helper/spy-helper.h',
        ]

    if bld.env['ENABLE_EXAMPLES']:
        bld.recurse

    # bld.ns3_python_bindings()

