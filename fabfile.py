import fabric.api as fab


@fab.task
def auto_build():
    fab.local('find src -type f | entr platformio run')
