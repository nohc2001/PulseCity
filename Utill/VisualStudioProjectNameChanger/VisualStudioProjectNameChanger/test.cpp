#include <Windows.h>
#include <iostream>

bool RenameFolder(const std::wstring& oldPath, const std::wstring& newPath) {
    // MoveFile �Լ��� ����Ͽ� ���� �̸� ����
    if (MoveFile(oldPath.c_str(), newPath.c_str())) {
        std::wcout << L"���� �̸��� ���������� ����Ǿ����ϴ�." << std::endl;
        return true;
    }
    else {
        DWORD error = GetLastError();
        std::wcerr << L"���� �̸� ���� ����. ���� �ڵ�: " << error << std::endl;
        return false;
    }
}

int main() {
    std::wstring oldFolderName = L"C:\\Users\\nohc2\\OneDrive\\Desktop\\WorkRoom\\Dev\\CollegeProjects\\DumyProject";
    std::wstring newFolderName = L"C:\\Users\\nohc2\\OneDrive\\Desktop\\WorkRoom\\Dev\\CollegeProjects\\Dumy2";

    if (RenameFolder(oldFolderName, newFolderName)) {
        // ���������� ���� �̸��� �����
    }

    return 0;
}